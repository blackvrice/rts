#include "game/game/systems/EnemyAiController.hpp"

#include <optional>

#include "core/data/BuildingStaticData.hpp"
#include "core/data/UnitStaticData.hpp"
#include "core/model/Building.hpp"
#include "core/model/ResourceNode.hpp"
#include "core/model/Unit.hpp"
#include "core/world/GameWorld.hpp"
#include "core/world/WorldRuntimeServices.hpp"
#include "game/game/systems/ConstructionSystem.hpp"
#include "game/game/systems/GameplayQueries.hpp"
#include "game/game/systems/MovementSystem.hpp"
#include "game/game/systems/ProductionSystem.hpp"

namespace rts::core::manager {
    namespace {
        constexpr float kAiProduceInterval = 5.0f;   // train workers/warriors cadence
        constexpr float kAiGatherInterval = 3.0f;     // assign idle workers cadence
        constexpr float kAiDefenseInterval = 1.0f;    // tactical threat scan cadence
        constexpr float kAiWaveInterval = 45.0f;       // send a wave even if undersized
        constexpr int   kAiMaxWorkers = 6;             // economy worker cap
        constexpr int   kAiWaveArmySize = 6;           // launch once this many idle soldiers mass
        constexpr float kAiDefenseRadius = 420.0f;
    }

    void EnemyAiController::reset() {
        m_produceTimer = 0.f;
        m_gatherTimer = 0.f;
        m_waveTimer = 0.f;
        m_defenseTimer = 0.f;
        m_state = BuildOrderState::Opening;
        m_lastFeedbackState = BuildOrderState::Opening;
    }

    nlohmann::json EnemyAiController::toJson() const {
        return nlohmann::json {
            { "state", static_cast<int>(m_state) },
            { "produceTimer", m_produceTimer },
            { "gatherTimer", m_gatherTimer },
            { "waveTimer", m_waveTimer },
            { "defenseTimer", m_defenseTimer }
        };
    }

    void EnemyAiController::fromJson(const nlohmann::json& ai) {
        m_state = static_cast<BuildOrderState>(ai.value("state", 0));
        m_produceTimer = ai.value("produceTimer", 0.0f);
        m_gatherTimer = ai.value("gatherTimer", 0.0f);
        m_waveTimer = ai.value("waveTimer", 0.0f);
        m_defenseTimer = ai.value("defenseTimer", 0.0f);
        m_lastFeedbackState = m_state;
    }

    std::shared_ptr<model::Unit> EnemyAiController::findIdleWorker(
        const world::GameWorld& world, const int teamId) {
        for (const auto& element : world.getElements()) {
            auto unit = std::dynamic_pointer_cast<model::Unit>(element);
            if (unit &&
                unit->getTeamId() == teamId &&
                unit->isWorker() &&
                unit->getAction() == model::ActionType::Idle) {
                return unit;
            }
        }
        return nullptr;
    }

    int EnemyAiController::countCombatUnits(
        const world::GameWorld& world, const int teamId, const bool idleOnly) {
        int count = 0;
        for (const auto& element : world.getElements()) {
            auto unit = std::dynamic_pointer_cast<model::Unit>(element);
            if (unit &&
                unit->getTeamId() == teamId &&
                !unit->isWorker() &&
                unit->getAction() != model::ActionType::Dead &&
                (!idleOnly || unit->getAction() == model::ActionType::Idle)) {
                ++count;
            }
        }
        return count;
    }

    model::Vector2D EnemyAiController::rallyPoint(const world::GameWorld& world) {
        const auto enemyHall = queries::findBuilding(world, model::TeamId::Enemy, model::BuildingType::TownHall);
        const auto playerHall = queries::findBuilding(world, model::TeamId::Player, model::BuildingType::TownHall);
        if (enemyHall && playerHall) {
            const auto a = enemyHall->getPosition();
            const auto b = playerHall->getPosition();
            return { a.x * 0.65f + b.x * 0.35f, a.y * 0.65f + b.y * 0.35f };
        }
        return enemyHall ? enemyHall->getPosition() : model::Vector2D{ 0.0f, 0.0f };
    }

    bool EnemyAiController::tryStartBarracks(world::GameWorld& world, ProductionSystem& production) {
        if (queries::findBuilding(world, model::TeamId::Enemy, model::BuildingType::Barracks)) {
            return false;
        }
        auto worker = findIdleWorker(world, model::TeamId::Enemy);
        auto townHall = queries::findBuilding(world, model::TeamId::Enemy, model::BuildingType::TownHall);
        if (!worker || !townHall) {
            return false;
        }

        const auto data = core::data::buildingStaticDataFor(model::BuildingType::Barracks);
        if (!queries::hasBuildingRequirements(world, model::TeamId::Enemy, data)) {
            return false;
        }

        auto resources = world.playerResources(model::TeamId::Enemy);
        if (!resources.canAfford(data.cost())) {
            return false;
        }

        const auto sitePos = ConstructionSystem::findBuildSiteNear(world, townHall->getPosition(), data);
        if (!sitePos) {
            return false;
        }

        auto site = std::make_shared<model::Building>(
            model::BuildingType::Barracks, *sitePos, model::TeamId::Enemy);
        site->beginConstruction(data.buildTimeSeconds, data.maxHp * 0.1f);
        site->setRallyPoint(rallyPoint(world));
        production.registerBuildingSpawn(*site);
        world.addElement(site);
        resources.pay(data.cost());
        world.setPlayerResources(model::TeamId::Enemy, resources);
        worker->buildAt(site.get());
        world::emitEffect(world, world::EffectType::ConstructionDust, *sitePos, 72.0f, 0.65f);
        return true;
    }

    void EnemyAiController::emitStateFeedback(world::GameWorld& world) {
        if (m_state == m_lastFeedbackState) {
            return;
        }
        m_lastFeedbackState = m_state;
        if (m_state == BuildOrderState::Attack) {
            world::emitSound(world, world::SoundCue::AttackOrder, rallyPoint(world), 35.0f);
        }
    }

    void EnemyAiController::update(
        world::GameWorld& world, MovementSystem& movement, ProductionSystem& production,
        const float dt, const bool passive) {
        if (world.gameResult() != world::GameResult::InProgress) {
            return;
        }

        updateBuildOrder(world, production);
        updateProduction(world, dt);
        updateWorkers(world, dt);
        // Passive mode (MatchConfig debug switch) skips the offensive passes so
        // the enemy never sorties or waves against the player.
        if (!passive) {
            updateDefense(world, movement, dt);
            updateWaves(world, movement, dt);
        }
    }

    void EnemyAiController::updateBuildOrder(world::GameWorld& world, ProductionSystem& production) {
        const auto barracks = queries::findBuilding(world, model::TeamId::Enemy, model::BuildingType::Barracks);
        const int combat = countCombatUnits(world, model::TeamId::Enemy, false);
        if (!barracks) {
            m_state = combat > 0 ? BuildOrderState::Rebuild : BuildOrderState::BuildBarracks;
            tryStartBarracks(world, production);
        } else if (combat >= kAiWaveArmySize) {
            m_state = BuildOrderState::Attack;
        } else if (combat > 0 ||
                   queries::findBuilding(world, model::TeamId::Enemy, model::BuildingType::Barracks, true)) {
            m_state = BuildOrderState::ProduceArmy;
        } else {
            m_state = BuildOrderState::Gather;
        }
        emitStateFeedback(world);
    }

    void EnemyAiController::updateProduction(world::GameWorld& world, const float dt) {
        m_produceTimer += dt;
        if (m_produceTimer < kAiProduceInterval) {
            return;
        }
        m_produceTimer = 0.f;

        auto resources = world.playerResources(model::TeamId::Enemy);

        int workerCount = 0;
        for (const auto& element : world.getElements()) {
            auto unit = std::dynamic_pointer_cast<model::Unit>(element);
            if (unit && unit->getTeamId() == model::TeamId::Enemy &&
                unit->isWorker() && unit->getAction() != model::ActionType::Dead) {
                ++workerCount;
            }
        }

        // Train from idle production buildings, paying from the enemy pool. Workers
        // grow the economy up to a cap; barracks keep warriors coming for waves.
        for (const auto& element : world.getElements()) {
            auto building = std::dynamic_pointer_cast<model::Building>(element);
            if (!building ||
                building->getTeamId() != model::TeamId::Enemy ||
                !building->isComplete() ||
                building->getAction() == model::ActionType::Dead ||
                building->trainQueueSize() > 0) {
                continue;
            }

            ::rts::UnitType unitType;
            if (building->buildingType() == model::BuildingType::TownHall) {
                if (workerCount >= kAiMaxWorkers) continue;
                unitType = ::rts::UnitType::Worker;
            } else {
                building->setRallyPoint(rallyPoint(world));
                unitType = ProductionSystem::defaultUnitFor(building->buildingType());
            }

            const auto cost = core::data::unitStaticDataFor(unitType).cost();
            if (resources.canAfford(cost) && building->trainUnit(unitType)) {
                resources.pay(cost);
                if (unitType == ::rts::UnitType::Worker) {
                    ++workerCount;
                }
            }
        }

        world.setPlayerResources(model::TeamId::Enemy, resources);
    }

    void EnemyAiController::updateWorkers(world::GameWorld& world, const float dt) {
        m_gatherTimer += dt;
        if (m_gatherTimer < kAiGatherInterval) {
            return;
        }
        m_gatherTimer = 0.f;

        for (const auto& element : world.getElements()) {
            auto worker = std::dynamic_pointer_cast<model::Unit>(element);
            if (!worker ||
                worker->getTeamId() != model::TeamId::Enemy ||
                !worker->isWorker() ||
                worker->getAction() != model::ActionType::Idle) {
                continue;
            }

            auto resource = queries::findClosestAvailableResource(
                world,
                world.playerResources(model::TeamId::Enemy).wood < 120
                    ? model::ResourceNode::ResourceType::Wood
                    : model::ResourceNode::ResourceType::Gold,
                *worker);
            if (!resource) {
                resource = queries::findClosestAvailableResource(
                    world, model::ResourceNode::ResourceType::Wood, *worker);
            }
            if (!resource) {
                continue;
            }

            auto dropOff = queries::findClosestDropOffFor(world, *worker);
            if (dropOff) {
                worker->gather(resource.get(), dropOff.get());
            }
        }
    }

    void EnemyAiController::updateDefense(world::GameWorld& world, MovementSystem& movement, const float dt) {
        m_defenseTimer += dt;
        if (m_defenseTimer < kAiDefenseInterval) {
            return;
        }
        m_defenseTimer = 0.0f;

        std::optional<model::Vector2D> threatPos;
        for (const auto& element : world.getElements()) {
            auto building = std::dynamic_pointer_cast<model::Building>(element);
            if (!building ||
                building->getTeamId() != model::TeamId::Enemy ||
                building->getAction() == model::ActionType::Dead) {
                continue;
            }
            for (const auto& nearby : world::queryRadius(
                     world, building->getPosition(), kAiDefenseRadius)) {
                auto unit = std::dynamic_pointer_cast<model::Unit>(nearby);
                if (unit &&
                    unit->getTeamId() == model::TeamId::Player &&
                    !unit->isWorker() &&
                    unit->getAction() != model::ActionType::Dead) {
                    threatPos = unit->getPosition();
                    break;
                }
            }
            if (threatPos) break;
        }

        if (!threatPos) {
            return;
        }

        for (const auto& element : world.getElements()) {
            auto unit = std::dynamic_pointer_cast<model::Unit>(element);
            if (unit &&
                unit->getTeamId() == model::TeamId::Enemy &&
                !unit->isWorker() &&
                unit->getAction() == model::ActionType::Idle) {
                movement.issueAttackMove(world, *unit, *threatPos);
            }
        }
        world::emitSound(world, world::SoundCue::AttackOrder, *threatPos, 34.0f);
    }

    void EnemyAiController::updateWaves(world::GameWorld& world, MovementSystem& movement, const float dt) {
        m_waveTimer += dt;

        int idleSoldiers = 0;
        for (const auto& element : world.getElements()) {
            auto unit = std::dynamic_pointer_cast<model::Unit>(element);
            if (unit &&
                unit->getTeamId() == model::TeamId::Enemy &&
                !unit->isWorker() &&
                unit->getAction() == model::ActionType::Idle) {
                ++idleSoldiers;
            }
        }

        // Attack once a real force has massed, or after the timeout so a stalled
        // economy still eventually pressures the player.
        const bool massReady = idleSoldiers >= kAiWaveArmySize;
        const bool timedOut = m_waveTimer >= kAiWaveInterval;
        if ((!massReady && !timedOut) || idleSoldiers == 0) {
            if (idleSoldiers > 0) {
                const auto rally = rallyPoint(world);
                for (const auto& element : world.getElements()) {
                    auto unit = std::dynamic_pointer_cast<model::Unit>(element);
                    if (unit &&
                        unit->getTeamId() == model::TeamId::Enemy &&
                        !unit->isWorker() &&
                        unit->getAction() == model::ActionType::Idle &&
                        queries::distanceSq(unit->getPosition(), rally) > 80.0f * 80.0f) {
                        movement.issueMove(world, *unit, rally, false);
                    }
                }
            }
            return;
        }
        m_waveTimer = 0.f;

        auto target = queries::findBuilding(world, model::TeamId::Player, model::BuildingType::TownHall);
        if (!target) {
            return;
        }
        for (const auto& element : world.getElements()) {
            auto unit = std::dynamic_pointer_cast<model::Unit>(element);
            if (unit &&
                unit->getTeamId() == model::TeamId::Enemy &&
                !unit->isWorker() &&
                unit->getAction() == model::ActionType::Idle) {
                movement.issueAttackMove(world, *unit, target->getPosition());
            }
        }
        world::emitSound(world, world::SoundCue::AttackOrder, target->getPosition(), 42.0f);
    }
}
