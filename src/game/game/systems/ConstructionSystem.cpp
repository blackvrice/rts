#include "game/game/systems/ConstructionSystem.hpp"

#include <algorithm>
#include <cstdlib>
#include <memory>

#include "core/data/BuildingStaticData.hpp"
#include "core/model/Building.hpp"
#include "core/model/Unit.hpp"
#include "core/world/GameWorld.hpp"
#include "core/world/WorldRuntimeServices.hpp"
#include "game/game/systems/GameplayQueries.hpp"
#include "game/game/systems/MovementSystem.hpp"
#include "game/game/systems/ProductionSystem.hpp"

namespace rts::core::manager {
    bool ConstructionSystem::canPlaceBuilding(
        const world::GameWorld& world, const int originX, const int originY, const int w, const int h) {
        for (int dy = 0; dy < h; ++dy) {
            for (int dx = 0; dx < w; ++dx) {
                const int gx = originX + dx;
                const int gy = originY + dy;
                if (world.isTileBlocked(gx, gy) || world.isCellOccupied(gx, gy)) {
                    return false;
                }
            }
        }
        return true;
    }

    model::Vector2D ConstructionSystem::footprintCenter(
        const world::GameWorld& world, const int originX, const int originY,
        const data::BuildingStaticData& data) {
        // Origin tile center minus half a tile is the footprint's top-left corner;
        // adding half the footprint size lands on its center.
        const auto& tf = world.gridTransform();
        const float halfW = data.footprintWidth * tf.tileSize * 0.5f;
        const float halfH = data.footprintHeight * tf.tileSize * 0.5f;
        const model::Vector2D originCenter = tf.gridToWorldCenter(path::GridPos{ originX, originY });
        return model::Vector2D {
            originCenter.x - tf.tileSize * 0.5f + halfW,
            originCenter.y - tf.tileSize * 0.5f + halfH
        };
    }

    std::optional<model::Vector2D> ConstructionSystem::findBuildSiteNear(
        const world::GameWorld& world, const model::Vector2D& center,
        const data::BuildingStaticData& data) {
        const auto& tf = world.gridTransform();
        const auto centerCell = tf.worldToGrid(center);
        for (int radius = 5; radius <= 14; ++radius) {
            for (int dy = -radius; dy <= radius; ++dy) {
                for (int dx = -radius; dx <= radius; ++dx) {
                    if (std::max(std::abs(dx), std::abs(dy)) != radius) {
                        continue;
                    }
                    const int originX = centerCell.x + dx - data.footprintWidth / 2;
                    const int originY = centerCell.y + dy - data.footprintHeight / 2;
                    if (!canPlaceBuilding(world, originX, originY, data.footprintWidth, data.footprintHeight)) {
                        continue;
                    }
                    return footprintCenter(world, originX, originY, data);
                }
            }
        }
        return std::nullopt;
    }

    void ConstructionSystem::orderWorkerToBuild(
        world::GameWorld& world,
        ProductionSystem& production,
        MovementSystem& movement,
        model::Unit& worker,
        const int buildingTypeId,
        const model::Vector2D& position) {
        worker.clearOrderQueue();
        movement.cancelQueuedPath(worker);

        // Resolve the requested building type; reject ids outside the known range.
        if (buildingTypeId < 0) {
            return;
        }
        const auto buildingType = static_cast<model::BuildingType>(buildingTypeId);
        const auto data = core::data::buildingStaticDataFor(buildingType);

        // Reject the build when a prerequisite building is missing for the team.
        if (!queries::hasBuildingRequirements(world, worker.getTeamId(), data)) {
            return;
        }

        // Center the footprint on the cursor tile.
        const auto centerCell = world.gridTransform().worldToGrid(position);
        const int originX = centerCell.x - data.footprintWidth / 2;
        const int originY = centerCell.y - data.footprintHeight / 2;

        if (!canPlaceBuilding(world, originX, originY, data.footprintWidth, data.footprintHeight)) {
            return;
        }

        auto resources = world.playerResources(worker.getTeamId());
        if (!resources.canAfford(data.cost())) {
            world::emitSound(world, world::SoundCue::ResourceShortage,
                             worker.getPosition(), 70.0f);
            return;
        }

        const model::Vector2D buildingPos = footprintCenter(world, originX, originY, data);

        auto site = std::make_shared<model::Building>(buildingType, buildingPos, worker.getTeamId());
        // Start as a fragile shell; workers raise HP/progress toward completion.
        site->beginConstruction(data.buildTimeSeconds, data.maxHp * 0.1f);
        production.registerBuildingSpawn(*site);
        world.addElement(site);
        world::emitSound(world, world::SoundCue::ConstructionComplete,
                         buildingPos, 38.0f);
        world::emitEffect(world, world::EffectType::ConstructionDust,
                          buildingPos, 72.0f, 0.65f);

        resources.pay(data.cost());
        world.setPlayerResources(worker.getTeamId(), resources);

        worker.buildAt(site.get());
    }
}
