#include "game/game/GameLogicManager.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <memory>
#include <vector>

#include <core/model/Unit.hpp>
#include <core/model/UnitType.hpp>
#include <core/model/Building.hpp>
#include <core/model/PlayerResourceState.hpp>
#include <core/model/ResourceNode.hpp>
#include <core/data/UnitStaticData.hpp>
#include <core/data/BuildingStaticData.hpp>
#include <core/world/GameWorld.hpp>

namespace {
    constexpr float kAttackTargetPickRadius = 64.0f;
    constexpr float kAttackTargetPickRadiusSq = kAttackTargetPickRadius * kAttackTargetPickRadius;

    float distanceSq(
        const rts::core::model::Vector2D& a,
        const rts::core::model::Vector2D& b) {
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        return dx * dx + dy * dy;
    }

    bool selectionContains(
        const rts::core::manager::SelectionSystem::SelectedList& selected,
        const rts::core::model::IGameElement* candidate) {
        for (const auto& weak : selected) {
            if (auto element = weak.lock(); element.get() == candidate) {
                return true;
            }
        }

        return false;
    }

    bool isOpposingTeam(const int attackerTeamId, const int targetTeamId) {
        return attackerTeamId != rts::core::model::TeamId::Neutral &&
               targetTeamId != rts::core::model::TeamId::Neutral &&
               attackerTeamId != targetTeamId;
    }
}

namespace rts::core::manager {
    GameLogicManager::GameLogicManager(
        command::LogicCommandBus &bus,
        command::LogicCommandRouter &router,
        core::world::GameWorld &world)
        : m_world(world), ILogicManager(bus, router) {
        // Temporary debug units until spawning/production owns initial world population.
        {
            auto lock = m_world.acquireWriteLock();

            core::model::PlayerResourceState playerResources {};
            playerResources.gold = 0;
            playerResources.wood = 0;
            playerResources.foodUsed = 2;
            playerResources.foodCapacity = 10;
            playerResources.army = 2;
            m_world.setPlayerResources(core::model::TeamId::Player, playerResources);

            core::model::PlayerResourceState enemyResources {};
            enemyResources.gold = 0;
            enemyResources.wood = 0;
            enemyResources.foodUsed = 2;
            enemyResources.foodCapacity = 10;
            enemyResources.army = 2;
            m_world.setPlayerResources(core::model::TeamId::Enemy, enemyResources);
            
            // --- Player Units ---
            auto worker = std::make_shared<core::model::Unit>(::rts::UnitType::Worker);
            worker->setPosition({300.f, 240.f});
            worker->setTeamId(core::model::TeamId::Player);
            m_world.addElement(worker);

            auto unit3 = std::make_shared<core::model::Unit>();
            unit3->setPosition({360.f, 300.f});
            unit3->setTeamId(core::model::TeamId::Player);
            m_world.addElement(unit3);

            // --- Enemy Units ---
            auto unit2 = std::make_shared<core::model::Unit>();
            unit2->setPosition({600.f, 500.f});
            unit2->setTeamId(core::model::TeamId::Enemy);
            m_world.addElement(unit2);

            auto unit4 = std::make_shared<core::model::Unit>();
            unit4->setPosition({650.f, 500.f});
            unit4->setTeamId(core::model::TeamId::Enemy);
            m_world.addElement(unit4);

            // --- Buildings ---
            auto townHall = std::make_shared<core::model::Building>(
                core::model::BuildingType::TownHall,
                core::model::Vector2D{220.f, 220.f},
                core::model::TeamId::Player
            );
            registerBuildingSpawn(*townHall);
            m_world.addElement(townHall);

            auto enemyTownHall = std::make_shared<core::model::Building>(
                core::model::BuildingType::TownHall,
                core::model::Vector2D{760.f, 650.f},
                core::model::TeamId::Enemy
            );
            registerBuildingSpawn(*enemyTownHall);
            m_world.addElement(enemyTownHall);

            auto enemyBarracks = std::make_shared<core::model::Building>(
                core::model::BuildingType::Barracks,
                core::model::Vector2D{700.f, 560.f},
                core::model::TeamId::Enemy
            );
            registerBuildingSpawn(*enemyBarracks);
            m_world.addElement(enemyBarracks);

            // --- Resources ---
            auto goldMine = std::make_shared<core::model::ResourceNode>(
                core::model::Vector2D{120.f, 180.f},
                core::model::ResourceNode::ResourceType::Gold,
                5000
            );
            m_world.addElement(goldMine);

            auto woodForest = std::make_shared<core::model::ResourceNode>(
                core::model::Vector2D{120.f, 360.f},
                core::model::ResourceNode::ResourceType::Wood,
                2000
            );
            m_world.addElement(woodForest);
        }

        m_router.on<command::SelectCommand>([this](const command::SelectCommand &cmd) {
            auto lock = m_world.acquireWriteLock();
            m_selection.selectInArea(m_world, cmd.area());
        });

        m_router.on<command::MoveCommand>([this](const command::MoveCommand &cmd) {
            handleMoveCommand(cmd);
        });

        m_router.on<command::AttackCommand>([this](const command::AttackCommand &cmd) {
            handleAttackCommand(cmd);
        });

        m_router.on<command::GatherCommand>([this](const command::GatherCommand &cmd) {
            handleGatherCommand(cmd);
        });

        m_router.on<command::TrainUnitCommand>([this](const command::TrainUnitCommand &cmd) {
            handleTrainCommand(cmd);
        });

        m_router.on<command::CancelProductionCommand>([this](const command::CancelProductionCommand &cmd) {
            handleCancelProduction(cmd);
        });

        m_router.on<command::BuildCommand>([this](const command::BuildCommand &cmd) {
            handleBuildCommand(cmd);
        });

        m_router.on<command::StopCommand>([this](const command::StopCommand &) {
            auto lock = m_world.acquireWriteLock();
            for (auto &weak: m_selection.selected()) {
                if (auto element = weak.lock(); element && element->getAction() != model::ActionType::Dead) {
                    element->stop();
                }
            }
        });

        m_router.on<command::HoldPositionCommand>([this](const command::HoldPositionCommand &) {
            auto lock = m_world.acquireWriteLock();
            for (auto &weak: m_selection.selected()) {
                if (auto element = weak.lock(); element && element->getAction() != model::ActionType::Dead) {
                    element->holdPosition();
                }
            }
        });

        m_router.on<command::PatrolCommand>([this](const command::PatrolCommand &) {
            // TODO
        });

        m_router.on<command::ControlGroupAddCommand>([this](const auto &cmd) {
            m_controlGroups.add(cmd.groupId(), m_selection.selected());
        });

        m_router.on<command::ControlGroupAssignCommand>([this](const auto &cmd) {
            m_controlGroups.assign(cmd.groupId(), m_selection.selected());
        });

        m_router.on<command::ControlGroupSelectCommand>([this](const auto &cmd) {
            auto lock = m_world.acquireWriteLock();
            m_selection.replaceSelected(m_controlGroups.select(cmd.groupId()));
        });
    }

    void GameLogicManager::update() {
        // Logic-level update hook for future AI and state transitions.
    }

    void GameLogicManager::tick(float dt) {
        auto lock = m_world.acquireWriteLock();
        m_movement.update(m_world, dt, m_collision);
        applyReadyResourceDeliveries();
        handleGatherRedirects();
        flushPendingSpawns();
    }

    void GameLogicManager::selectElement(core::model::IGameElement &element) {
        m_selection.selectElement(element);
    }

    void GameLogicManager::addSelectedElement(core::model::IGameElement &element) {
        m_selection.addSelectedElement(element);
    }

    void GameLogicManager::clearSelection() {
        m_selection.clear();
    }

    bool GameLogicManager::canMoveUnitTo(
        const core::model::Unit &unit,
        const core::model::Vector2D &pos) const {
        return m_collision.canMoveUnitTo(m_world, unit, pos);
    }

    void GameLogicManager::handleMoveCommand(const command::MoveCommand& cmd) {
        auto lock = m_world.acquireWriteLock();
        // A move order on a selected production building sets its rally point instead
        // of moving the (immobile) building.
        for (const auto& weak : m_selection.selected()) {
            if (auto building = std::dynamic_pointer_cast<model::Building>(weak.lock());
                building && building->getAction() != model::ActionType::Dead) {
                building->setRallyPoint(cmd.target());
            }
        }
        m_movement.issueMove(m_world, m_selection.selected(), cmd.target());
    }

    std::shared_ptr<model::IGameElement> GameLogicManager::findCommandTargetAt(
        const model::Vector2D& target,
        const SelectionSystem::SelectedList& selected) const {
        std::shared_ptr<model::IGameElement> bestTarget;
        float bestDistanceSq = std::numeric_limits<float>::max();

        for (const auto& element : m_world.getElements()) {
            auto candidate = std::dynamic_pointer_cast<model::IGameElement>(element);
            if (!candidate ||
                candidate->getAction() == model::ActionType::Dead) {
                continue;
            }

            const bool alreadySelected = selectionContains(selected, candidate.get());
            const bool selectedResource =
                alreadySelected && std::dynamic_pointer_cast<model::ResourceNode>(candidate);
            // Drag-select can include resource nodes; workers still need the selected resource as a gather target.
            if (alreadySelected && !selectedResource) {
                continue;
            }

            const float candidateDistanceSq = distanceSq(candidate->getPosition(), target);
            if (candidateDistanceSq <= kAttackTargetPickRadiusSq &&
                candidateDistanceSq < bestDistanceSq) {
                bestDistanceSq = candidateDistanceSq;
                bestTarget = std::move(candidate);
            }
        }

        return bestTarget;
    }

    std::shared_ptr<model::Building> GameLogicManager::findClosestDropOffFor(const model::Unit& worker) const {
        std::shared_ptr<model::Building> bestDropOff;
        float bestDistanceSq = std::numeric_limits<float>::max();

        for (const auto& element : m_world.getElements()) {
            auto building = std::dynamic_pointer_cast<model::Building>(element);
            if (!building ||
                !building->isDropOff() ||
                building->getAction() == model::ActionType::Dead ||
                building->getTeamId() != worker.getTeamId()) {
                continue;
            }

            const float candidateDistanceSq = distanceSq(
                building->getPosition(),
                worker.getPosition()
            );
            if (candidateDistanceSq < bestDistanceSq) {
                bestDistanceSq = candidateDistanceSq;
                bestDropOff = std::move(building);
            }
        }

        return bestDropOff;
    }

    void GameLogicManager::issueGatherToResource(model::ResourceNode& resource) {
        if (resource.isDepleted()) {
            return;
        }

        for (const auto& weak : m_selection.selected()) {
            auto element = weak.lock();
            auto worker = std::dynamic_pointer_cast<model::Unit>(element);
            if (!worker ||
                !worker->isWorker() ||
                worker->getAction() == model::ActionType::Dead) {
                continue;
            }

            auto dropOff = findClosestDropOffFor(*worker);
            if (!dropOff) {
                worker->stop();
                continue;
            }

            worker->gather(&resource, dropOff.get());
        }
    }

    std::shared_ptr<model::ResourceNode> GameLogicManager::findClosestAvailableResource(
        model::ResourceNode::ResourceType type, const model::Unit& requester) const
    {
        std::shared_ptr<model::ResourceNode> best;
        float bestDistSq = std::numeric_limits<float>::max();

        for (const auto& element : m_world.getElements()) {
            auto node = std::dynamic_pointer_cast<model::ResourceNode>(element);
            if (!node || node->isDepleted() || node->type() != type) {
                continue;
            }
            if (node->reservedGathererCount() >= node->maxGatherers()) {
                continue;
            }
            const float dSq = distanceSq(requester.getPosition(), node->getPosition());
            if (dSq < bestDistSq) {
                bestDistSq = dSq;
                best = node;
            }
        }
        return best;
    }

    void GameLogicManager::handleGatherRedirects() {
        for (const auto& element : m_world.getElements()) {
            auto worker = std::dynamic_pointer_cast<model::Unit>(element);
            if (!worker || !worker->isWorker() ||
                worker->getAction() == model::ActionType::Dead) {
                continue;
            }

            if (worker->isNeedingDropOffRedirect()) {
                auto newDropOff = findClosestDropOffFor(*worker);
                if (newDropOff) {
                    worker->redirectToDropOff(newDropOff.get());
                } else {
                    worker->stop();
                }
            } else if (worker->isNeedingResourceRedirect()) {
                auto newResource = findClosestAvailableResource(
                    worker->targetGatherType(), *worker);
                auto newDropOff = findClosestDropOffFor(*worker);
                if (newResource && newDropOff) {
                    worker->gather(newResource.get(), newDropOff.get());
                } else {
                    worker->stop();
                }
            }
        }
    }

    void GameLogicManager::applyReadyResourceDeliveries() {
        for (const auto& element : m_world.getElements()) {
            auto unit = std::dynamic_pointer_cast<model::Unit>(element);
            if (!unit) {
                continue;
            }

            const auto delivery = unit->takeReadyResourceDelivery();
            if (!delivery || delivery->amount <= 0) {
                continue;
            }

            auto resources = m_world.playerResources(unit->getTeamId());
            switch (delivery->type) {
                case model::ResourceNode::ResourceType::Gold:
                    resources.gold += delivery->amount;
                    break;
                case model::ResourceNode::ResourceType::Wood:
                    resources.wood += delivery->amount;
                    break;
            }

            m_world.setPlayerResources(unit->getTeamId(), resources);
        }
    }

    void GameLogicManager::handleAttackCommand(const command::AttackCommand& cmd) {
        auto lock = m_world.acquireWriteLock();

        if (!cmd.hasWorldTarget()) {
            return;
        }

        int attackerTeam = model::TeamId::Neutral;
        for (const auto& weak : m_selection.selected()) {
            if (auto unit = weak.lock(); unit && unit->getAction() != model::ActionType::Dead) {
                attackerTeam = unit->getTeamId();
                break;
            }
        }

        const auto target = findCommandTargetAt(cmd.target(), m_selection.selected());
        if (!target) {
            m_movement.issueMove(m_world, m_selection.selected(), cmd.target());
            return;
        }

        if (auto resource = std::dynamic_pointer_cast<model::ResourceNode>(target)) {
            issueGatherToResource(*resource);
            return;
        }

        // Right-click follows RTS convention: friendly target means move/approach,
        // opposing team target means attack.
        if (!isOpposingTeam(attackerTeam, target->getTeamId())) {
            m_movement.issueMove(m_world, m_selection.selected(), target->getPosition());
            return;
        }

        for (const auto& weak : m_selection.selected()) {
            if (auto attacker = weak.lock();
                attacker &&
                attacker.get() != target.get() &&
                attacker->getAction() != model::ActionType::Dead) {
                attacker->attack(target.get());
            }
        }
    }

    void GameLogicManager::handleGatherCommand(const command::GatherCommand& cmd) {
        auto lock = m_world.acquireWriteLock();

        if (!cmd.hasWorldTarget()) {
            return;
        }

        const auto target = findCommandTargetAt(cmd.target(), m_selection.selected());
        auto resource = std::dynamic_pointer_cast<model::ResourceNode>(target);
        if (!resource) {
            return;
        }

        issueGatherToResource(*resource);
    }

    // =========================================================
    // Production
    // =========================================================
    ::rts::UnitType GameLogicManager::defaultUnitFor(model::BuildingType type) {
        switch (type) {
            case model::BuildingType::TownHall: return ::rts::UnitType::Worker;
            case model::BuildingType::Barracks: return ::rts::UnitType::Warrior;
        }
        return ::rts::UnitType::Warrior;
    }

    std::shared_ptr<model::Building> GameLogicManager::firstSelectedBuilding() const {
        for (const auto& weak : m_selection.selected()) {
            if (auto building = std::dynamic_pointer_cast<model::Building>(weak.lock());
                building && building->getAction() != model::ActionType::Dead) {
                return building;
            }
        }
        return nullptr;
    }

    void GameLogicManager::registerBuildingSpawn(model::Building& building) {
        // Buffer the spawn request; the actual element insertion happens in
        // flushPendingSpawns() after the per-tick element sweep completes.
        building.setUnitSpawnFn(
            [this](::rts::UnitType type, const model::Vector2D& anchor,
                   const model::Vector2D& rally, bool hasRally, int team) {
                m_pendingSpawns.push_back(PendingSpawn{ type, anchor, rally, hasRally, team });
            });
    }

    model::Vector2D GameLogicManager::findFreeSpawnPosition(const model::Vector2D& anchor) const {
        const auto& tf = m_world.gridTransform();
        const auto origin = tf.worldToGrid(anchor);

        // Expand outward ring by ring around the anchor cell until a walkable,
        // unoccupied tile is found; fall back to the anchor when none is free.
        constexpr int kMaxRadius = 6;
        for (int radius = 0; radius <= kMaxRadius; ++radius) {
            for (int dy = -radius; dy <= radius; ++dy) {
                for (int dx = -radius; dx <= radius; ++dx) {
                    // Only inspect the perimeter of the current ring.
                    if (std::max(std::abs(dx), std::abs(dy)) != radius) {
                        continue;
                    }
                    const int gx = origin.x + dx;
                    const int gy = origin.y + dy;
                    if (m_world.isTileBlocked(gx, gy) || m_world.isCellOccupied(gx, gy)) {
                        continue;
                    }
                    return tf.gridToWorldCenter(path::GridPos{ gx, gy });
                }
            }
        }
        return anchor;
    }

    void GameLogicManager::flushPendingSpawns() {
        if (m_pendingSpawns.empty()) {
            return;
        }

        // Move out first so spawned-unit side effects can't grow the buffer we iterate.
        std::vector<PendingSpawn> spawns;
        spawns.swap(m_pendingSpawns);

        for (const auto& spawn : spawns) {
            auto unit = std::make_shared<model::Unit>(spawn.type);
            unit->setPosition(findFreeSpawnPosition(spawn.anchor));
            unit->setTeamId(spawn.team);
            m_world.addElement(unit);

            if (spawn.hasRally) {
                unit->moveTo(spawn.rally);
            }
        }
    }

    void GameLogicManager::handleTrainCommand(const command::TrainUnitCommand& cmd) {
        auto lock = m_world.acquireWriteLock();

        auto building = firstSelectedBuilding();
        if (!building) {
            return;
        }

        const ::rts::UnitType unitType = cmd.unitTypeId() < 0
            ? defaultUnitFor(building->buildingType())
            : static_cast<::rts::UnitType>(cmd.unitTypeId());

        if (building->trainQueueSize() >= model::Building::kMaxTrainQueue) {
            return;
        }

        const auto staticData = core::data::unitStaticDataFor(unitType);
        const auto cost = staticData.cost();

        auto resources = m_world.playerResources(building->getTeamId());
        if (!resources.canAfford(cost)) {
            return;
        }

        if (!building->trainUnit(unitType)) {
            return;
        }

        resources.pay(cost);
        m_world.setPlayerResources(building->getTeamId(), resources);
    }

    void GameLogicManager::handleCancelProduction(const command::CancelProductionCommand& cmd) {
        auto lock = m_world.acquireWriteLock();

        auto building = firstSelectedBuilding();
        if (!building) {
            return;
        }

        const auto cancelled = building->cancelLastTrain();
        if (!cancelled) {
            return;
        }

        // Refund the cancelled unit's cost to the owning player.
        const auto staticData = core::data::unitStaticDataFor(*cancelled);
        auto resources = m_world.playerResources(building->getTeamId());
        resources.refund(staticData.cost());
        m_world.setPlayerResources(building->getTeamId(), resources);
    }

    // =========================================================
    // Construction
    // =========================================================
    std::shared_ptr<model::Unit> GameLogicManager::firstSelectedWorker() const {
        for (const auto& weak : m_selection.selected()) {
            if (auto unit = std::dynamic_pointer_cast<model::Unit>(weak.lock());
                unit && unit->isWorker() && unit->getAction() != model::ActionType::Dead) {
                return unit;
            }
        }
        return nullptr;
    }

    bool GameLogicManager::canPlaceBuilding(int originX, int originY, int w, int h) const {
        for (int dy = 0; dy < h; ++dy) {
            for (int dx = 0; dx < w; ++dx) {
                const int gx = originX + dx;
                const int gy = originY + dy;
                if (m_world.isTileBlocked(gx, gy) || m_world.isCellOccupied(gx, gy)) {
                    return false;
                }
            }
        }
        return true;
    }

    void GameLogicManager::handleBuildCommand(const command::BuildCommand& cmd) {
        auto lock = m_world.acquireWriteLock();

        auto worker = firstSelectedWorker();
        if (!worker) {
            return;
        }

        // Resolve the requested building type; reject ids outside the known range.
        if (cmd.buildingTypeId() < 0) {
            return;
        }
        const auto buildingType = static_cast<model::BuildingType>(cmd.buildingTypeId());
        const auto data = core::data::buildingStaticDataFor(buildingType);

        // Center the footprint on the cursor tile.
        const auto& tf = m_world.gridTransform();
        const auto centerCell = tf.worldToGrid(cmd.position());
        const int originX = centerCell.x - data.footprintWidth / 2;
        const int originY = centerCell.y - data.footprintHeight / 2;

        if (!canPlaceBuilding(originX, originY, data.footprintWidth, data.footprintHeight)) {
            return;
        }

        auto resources = m_world.playerResources(worker->getTeamId());
        if (!resources.canAfford(data.cost())) {
            return;
        }

        // Place the building at the footprint center so its single occupied cell and
        // sprite line up with the validated region.
        const float halfW = data.footprintWidth * tf.tileSize * 0.5f;
        const float halfH = data.footprintHeight * tf.tileSize * 0.5f;
        const model::Vector2D buildingPos {
            tf.gridToWorldCenter(path::GridPos{ originX, originY }).x - tf.tileSize * 0.5f + halfW,
            tf.gridToWorldCenter(path::GridPos{ originX, originY }).y - tf.tileSize * 0.5f + halfH
        };

        auto site = std::make_shared<model::Building>(buildingType, buildingPos, worker->getTeamId());
        // Start as a fragile shell; workers raise HP/progress toward completion.
        site->beginConstruction(data.buildTimeSeconds, data.maxHp * 0.1f);
        registerBuildingSpawn(*site);
        m_world.addElement(site);

        resources.pay(data.cost());
        m_world.setPlayerResources(worker->getTeamId(), resources);

        worker->buildAt(site.get());
    }
}
