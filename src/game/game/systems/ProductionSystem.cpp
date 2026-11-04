#include "game/game/systems/ProductionSystem.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <memory>

#include "core/data/DataRegistry.hpp"
#include "core/data/UnitStaticData.hpp"
#include "core/model/Building.hpp"
#include "core/model/Unit.hpp"
#include "core/tech/TechTreeValidator.hpp"
#include "core/world/GameWorld.hpp"
#include "core/world/WorldRuntimeServices.hpp"
#include "game/game/systems/GameplayQueries.hpp"
#include "game/game/systems/MovementSystem.hpp"

namespace rts::core::manager {
    ::rts::UnitType ProductionSystem::defaultUnitFor(const model::BuildingType type) {
        // The first entry of the building's produces list is its default unit.
        const auto& produces = data::DataRegistry::global().building(type).produces;
        return produces.empty() ? ::rts::UnitType::Warrior : produces.front();
    }

    void ProductionSystem::registerBuildingSpawn(model::Building& building) {
        // Buffer the spawn request; the actual element insertion happens in
        // flushPendingSpawns() after the per-tick element sweep completes.
        building.setUnitSpawnFn(
            [this](::rts::UnitType type, const model::Vector2D& anchor,
                   const model::Vector2D& rally, bool hasRally, int team) {
                m_pendingSpawns.push_back(PendingSpawn{ type, anchor, rally, hasRally, team });
            });
    }

    std::optional<model::Vector2D> ProductionSystem::findFreeSpawnPosition(
        const world::GameWorld& world,
        const model::Vector2D& anchor,
        const std::optional<model::Vector2D>& prefer) {
        const auto& tf = world.gridTransform();
        const auto origin = tf.worldToGrid(anchor);

        // Expand ring by ring around the anchor. Within a ring, pick the free tile
        // nearest the preferred point (rally) so units fan out toward their rally;
        // without a preference, the first free tile wins. nullopt = fully boxed in.
        constexpr int kMaxRadius = 6;
        for (int radius = 0; radius <= kMaxRadius; ++radius) {
            std::optional<model::Vector2D> best;
            float bestDistSq = std::numeric_limits<float>::max();
            for (int dy = -radius; dy <= radius; ++dy) {
                for (int dx = -radius; dx <= radius; ++dx) {
                    // Only inspect the perimeter of the current ring.
                    if (std::max(std::abs(dx), std::abs(dy)) != radius) {
                        continue;
                    }
                    const int gx = origin.x + dx;
                    const int gy = origin.y + dy;
                    if (world.isTileBlocked(gx, gy) || world.isCellOccupied(gx, gy)) {
                        continue;
                    }
                    const auto candidate = tf.gridToWorldCenter(path::GridPos{ gx, gy });
                    if (!prefer) {
                        return candidate;  // no preference: first free tile is fine
                    }
                    const float dSq = queries::distanceSq(candidate, *prefer);
                    if (dSq < bestDistSq) {
                        bestDistSq = dSq;
                        best = candidate;
                    }
                }
            }
            if (best) {
                return best;  // closest-to-rally free tile in the nearest occupied ring
            }
        }
        return std::nullopt;
    }

    bool ProductionSystem::isEnemyNear(
        const world::GameWorld& world, const model::Vector2D& point, const int team) {
        for (auto candidate : world::queryRadius(world, point, queries::kCommandPickRadius)) {
            if (!candidate ||
                candidate->getAction() == model::ActionType::Dead ||
                !world.areEnemies(team, candidate->getTeamId())) {
                continue;
            }
            if (queries::distanceSq(candidate->getPosition(), point) <= queries::kCommandPickRadiusSq) {
                return true;
            }
        }
        return false;
    }

    void ProductionSystem::flushPendingSpawns(world::GameWorld& world, MovementSystem& movement) {
        if (m_pendingSpawns.empty()) {
            return;
        }

        // Move out first so spawned-unit side effects can't grow the buffer we iterate.
        std::vector<PendingSpawn> spawns;
        spawns.swap(m_pendingSpawns);

        for (const auto& spawn : spawns) {
            const std::optional<model::Vector2D> prefer =
                spawn.hasRally ? std::optional{ spawn.rally } : std::nullopt;
            const auto position = findFreeSpawnPosition(world, spawn.anchor, prefer);
            if (!position) {
                // No room around the building yet — hold the unit and retry next tick.
                m_pendingSpawns.push_back(spawn);
                continue;
            }

            auto unit = std::make_shared<model::Unit>(spawn.type);
            unit->setPosition(*position);
            unit->setTeamId(spawn.team);
            world.addElement(unit);
            world::emitSound(world, world::SoundCue::ProductionComplete,
                             *position, spawn.team == world.localPlayerId() ? 62.0f : 34.0f);
            world::emitEffect(world, world::EffectType::ConstructionDust,
                              *position, 42.0f, 0.45f);

            if (spawn.hasRally) {
                // A rally point on/near an enemy means attack-move there instead of
                // a passive move, so rallied units engage on arrival.
                if (isEnemyNear(world, spawn.rally, spawn.team)) {
                    movement.issueAttackMove(world, *unit, spawn.rally);
                } else {
                    unit->moveTo(spawn.rally);
                }
            }
        }
    }

    void ProductionSystem::train(
        world::GameWorld& world, model::Building& building, const int unitTypeId) const {
        const ::rts::UnitType unitType = unitTypeId < 0
            ? defaultUnitFor(building.buildingType())
            : static_cast<::rts::UnitType>(unitTypeId);

        // Tech gate: the selected building enforces "produced here"; this also checks
        // the unit's own prerequisites (e.g. a future tech building).
        if (!tech::TechTreeValidator::canProduce(
                queries::buildTechState(world, building.getTeamId()), unitType).ok) {
            return;
        }

        if (building.trainQueueSize() >= model::Building::kMaxTrainQueue) {
            return;
        }

        const auto staticData = data::unitStaticDataFor(unitType);
        const auto cost = staticData.cost();

        auto resources = world.playerResources(building.getTeamId());
        if (!resources.canAfford(cost)) {
            world::emitSound(world, world::SoundCue::ResourceShortage,
                             building.getPosition(), 70.0f);
            return;
        }

        if (!building.trainUnit(unitType)) {
            return;
        }

        resources.pay(cost);
        world.setPlayerResources(building.getTeamId(), resources);
    }

    void ProductionSystem::cancelLast(world::GameWorld& world, model::Building& building) const {
        const auto cancelled = building.cancelLastTrain();
        if (!cancelled) {
            return;
        }

        // Refund the cancelled unit's cost to the owning player.
        const auto staticData = data::unitStaticDataFor(*cancelled);
        auto resources = world.playerResources(building.getTeamId());
        resources.refund(staticData.cost());
        world.setPlayerResources(building.getTeamId(), resources);
    }
}
