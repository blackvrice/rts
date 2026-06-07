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
    constexpr float kAttackMoveAcquireRadius = 220.0f;

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

            // Debug starting resources so production/construction can be exercised
            // immediately during manual playtests.
            core::model::PlayerResourceState playerResources {};
            playerResources.gold = 500;
            playerResources.wood = 300;
            playerResources.foodUsed = 0;
            playerResources.foodCapacity = 20;
            playerResources.army = 0;
            m_world.setPlayerResources(core::model::TeamId::Player, playerResources);

            core::model::PlayerResourceState enemyResources {};
            enemyResources.gold = 500;
            enemyResources.wood = 300;
            enemyResources.foodUsed = 0;
            enemyResources.foodCapacity = 20;
            enemyResources.army = 0;
            m_world.setPlayerResources(core::model::TeamId::Enemy, enemyResources);

            // Spawns one unit of every type for a team in a spaced-out row so the
            // initial placement never overlaps the unit collision radius (28px ->
            // 56px min distance); 110px columns keep a comfortable gap.
            const auto spawnUnitRow = [this](int teamId, float startX, float y) {
                const ::rts::UnitType types[] = {
                    ::rts::UnitType::Worker,
                    ::rts::UnitType::Warrior,
                    ::rts::UnitType::Archer,
                    ::rts::UnitType::Marine
                };
                float x = startX;
                for (const auto type : types) {
                    auto unit = std::make_shared<core::model::Unit>(type);
                    unit->setPosition({ x, y });
                    unit->setTeamId(teamId);
                    m_world.addElement(unit);
                    x += 110.f;
                }
            };

            // --- Player base (top-left) ---
            auto townHall = std::make_shared<core::model::Building>(
                core::model::BuildingType::TownHall,
                core::model::Vector2D{220.f, 220.f},
                core::model::TeamId::Player
            );
            registerBuildingSpawn(*townHall);
            m_world.addElement(townHall);

            auto playerBarracks = std::make_shared<core::model::Building>(
                core::model::BuildingType::Barracks,
                core::model::Vector2D{500.f, 220.f},
                core::model::TeamId::Player
            );
            registerBuildingSpawn(*playerBarracks);
            m_world.addElement(playerBarracks);

            spawnUnitRow(core::model::TeamId::Player, 240.f, 460.f);

            // --- Enemy base (bottom-right) ---
            auto enemyTownHall = std::make_shared<core::model::Building>(
                core::model::BuildingType::TownHall,
                core::model::Vector2D{1200.f, 760.f},
                core::model::TeamId::Enemy
            );
            registerBuildingSpawn(*enemyTownHall);
            m_world.addElement(enemyTownHall);

            auto enemyBarracks = std::make_shared<core::model::Building>(
                core::model::BuildingType::Barracks,
                core::model::Vector2D{1480.f, 760.f},
                core::model::TeamId::Enemy
            );
            registerBuildingSpawn(*enemyBarracks);
            m_world.addElement(enemyBarracks);

            spawnUnitRow(core::model::TeamId::Enemy, 1200.f, 1000.f);

            // --- Resources (left edge, clear of the base footprints) ---
            auto goldMine = std::make_shared<core::model::ResourceNode>(
                core::model::Vector2D{120.f, 620.f},
                core::model::ResourceNode::ResourceType::Gold,
                5000
            );
            m_world.addElement(goldMine);

            auto woodForest = std::make_shared<core::model::ResourceNode>(
                core::model::Vector2D{120.f, 760.f},
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

        m_router.on<command::AttackMoveCommand>([this](const command::AttackMoveCommand &cmd) {
            handleAttackMoveCommand(cmd);
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
            if (inputLocked()) return;
            m_movement.cancelQueuedPaths(m_selection.selected());
            for (auto &weak: m_selection.selected()) {
                if (auto element = weak.lock(); element && element->getAction() != model::ActionType::Dead) {
                    if (auto unit = std::dynamic_pointer_cast<model::Unit>(element)) {
                        unit->clearOrderQueue();
                    }
                    element->stop();
                }
            }
        });

        m_router.on<command::HoldPositionCommand>([this](const command::HoldPositionCommand &) {
            auto lock = m_world.acquireWriteLock();
            if (inputLocked()) return;
            m_movement.cancelQueuedPaths(m_selection.selected());
            for (auto &weak: m_selection.selected()) {
                if (auto element = weak.lock(); element && element->getAction() != model::ActionType::Dead) {
                    if (auto unit = std::dynamic_pointer_cast<model::Unit>(element)) {
                        unit->clearOrderQueue();
                    }
                    element->holdPosition();
                }
            }
        });

        m_router.on<command::PatrolCommand>([this](const command::PatrolCommand &cmd) {
            handlePatrolCommand(cmd);
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
        handleAttackMoveOrders();
        handlePatrolOrders();
        handleHoldPositionOrders();
        handleQueuedOrders();
        applyReadyResourceDeliveries();
        handleGatherRedirects();
        flushPendingSpawns();
        updateAI(dt);
        checkVictoryDefeat();
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
        if (inputLocked()) return;

        if (cmd.append()) {
            queueMoveOrderForSelected(cmd.target());
            return;
        }

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
                worker->clearOrderQueue();
                worker->stop();
                continue;
            }

            worker->clearOrderQueue();
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

    std::shared_ptr<model::IGameElement> GameLogicManager::findClosestAttackMoveTarget(
        const model::Unit& unit) const {
        std::shared_ptr<model::IGameElement> bestTarget;
        float bestDistanceSq = std::numeric_limits<float>::max();
        const float acquireRadius = std::max(kAttackMoveAcquireRadius, unit.getAttackRange());
        const float acquireRadiusSq = acquireRadius * acquireRadius;

        for (const auto& element : m_world.getElements()) {
            auto candidate = std::dynamic_pointer_cast<model::IGameElement>(element);
            if (!candidate ||
                candidate.get() == &unit ||
                candidate->getAction() == model::ActionType::Dead ||
                !isOpposingTeam(unit.getTeamId(), candidate->getTeamId())) {
                continue;
            }

            const float candidateDistanceSq = distanceSq(
                candidate->getPosition(),
                unit.getPosition()
            );
            if (candidateDistanceSq <= acquireRadiusSq &&
                candidateDistanceSq < bestDistanceSq) {
                bestDistanceSq = candidateDistanceSq;
                bestTarget = candidate;
            }
        }

        return bestTarget;
    }

    void GameLogicManager::handleAttackMoveOrders() {
        for (const auto& element : m_world.getElements()) {
            auto unit = std::dynamic_pointer_cast<model::Unit>(element);
            if (!unit ||
                unit->getAction() == model::ActionType::Dead ||
                !unit->isAttackMoveActive()) {
                continue;
            }

            if (unit->isAttackMoveSearching()) {
                if (auto target = findClosestAttackMoveTarget(*unit)) {
                    unit->attackMoveEngage(target.get());
                    continue;
                }
            }

            if (unit->needsAttackMoveResume()) {
                m_movement.issueAttackMove(m_world, *unit, unit->attackMoveTarget());
            } else if (unit->getAction() == model::ActionType::Idle) {
                unit->stop();
            }
        }
    }

    std::shared_ptr<model::IGameElement> GameLogicManager::findClosestPatrolTarget(
        const model::Unit& unit) const {
        std::shared_ptr<model::IGameElement> bestTarget;
        float bestDistanceSq = std::numeric_limits<float>::max();
        const float acquireRadius = std::max(kAttackMoveAcquireRadius, unit.getAttackRange());
        const float acquireRadiusSq = acquireRadius * acquireRadius;

        for (const auto& element : m_world.getElements()) {
            auto candidate = std::dynamic_pointer_cast<model::IGameElement>(element);
            if (!candidate ||
                candidate.get() == &unit ||
                candidate->getAction() == model::ActionType::Dead ||
                !isOpposingTeam(unit.getTeamId(), candidate->getTeamId())) {
                continue;
            }

            const float candidateDistanceSq = distanceSq(
                candidate->getPosition(),
                unit.getPosition()
            );
            if (candidateDistanceSq <= acquireRadiusSq &&
                candidateDistanceSq < bestDistanceSq) {
                bestDistanceSq = candidateDistanceSq;
                bestTarget = candidate;
            }
        }

        return bestTarget;
    }

    void GameLogicManager::handlePatrolOrders() {
        for (const auto& element : m_world.getElements()) {
            auto unit = std::dynamic_pointer_cast<model::Unit>(element);
            if (!unit ||
                unit->getAction() == model::ActionType::Dead ||
                !unit->isPatrolActive()) {
                continue;
            }

            if (unit->isPatrolSearching()) {
                if (auto target = findClosestPatrolTarget(*unit)) {
                    unit->patrolEngage(target.get());
                    continue;
                }
            }

            if (unit->needsPatrolResume()) {
                m_movement.issuePatrol(m_world, *unit, unit->patrolDestination());
            }
        }
    }

    std::shared_ptr<model::IGameElement> GameLogicManager::findClosestHoldTarget(
        const model::Unit& unit) const {
        std::shared_ptr<model::IGameElement> bestTarget;
        float bestDistanceSq = std::numeric_limits<float>::max();
        const float rangeSq = unit.getAttackRange() * unit.getAttackRange();

        for (const auto& element : m_world.getElements()) {
            auto candidate = std::dynamic_pointer_cast<model::IGameElement>(element);
            if (!candidate ||
                candidate.get() == &unit ||
                candidate->getAction() == model::ActionType::Dead ||
                !isOpposingTeam(unit.getTeamId(), candidate->getTeamId())) {
                continue;
            }

            const float candidateDistanceSq = distanceSq(
                candidate->getPosition(),
                unit.getPosition()
            );
            if (candidateDistanceSq <= rangeSq &&
                candidateDistanceSq < bestDistanceSq) {
                bestDistanceSq = candidateDistanceSq;
                bestTarget = candidate;
            }
        }

        return bestTarget;
    }

    void GameLogicManager::handleHoldPositionOrders() {
        for (const auto& element : m_world.getElements()) {
            auto unit = std::dynamic_pointer_cast<model::Unit>(element);
            if (!unit ||
                unit->getAction() == model::ActionType::Dead ||
                !unit->isHoldingPosition()) {
                continue;
            }

            if (auto target = findClosestHoldTarget(*unit)) {
                unit->holdEngage(target.get());
            }
        }
    }

    void GameLogicManager::clearSelectedUnitOrderQueues() {
        m_movement.cancelQueuedPaths(m_selection.selected());
        for (const auto& weak : m_selection.selected()) {
            auto unit = std::dynamic_pointer_cast<model::Unit>(weak.lock());
            if (unit && unit->getAction() != model::ActionType::Dead) {
                unit->clearOrderQueue();
            }
        }
    }

    void GameLogicManager::queueMoveOrderForSelected(const model::Vector2D& target) {
        for (const auto& assignment : m_movement.formationTargets(m_world, m_selection.selected(), target)) {
            auto unit = assignment.unit;
            if (!unit || unit->getAction() == model::ActionType::Dead) {
                continue;
            }

            unit->enqueueOrder(model::UnitOrder {
                .type = model::OrderType::Move,
                .targetPosition = assignment.target
            });

            // Shift-clicking while idle should still begin the first queued waypoint.
            if (unit->getAction() == model::ActionType::Idle) {
                issueNextQueuedOrder(*unit);
            }
        }
    }

    void GameLogicManager::issueNextQueuedOrder(model::Unit& unit) {
        while (auto order = unit.popNextOrder()) {
            switch (order->type) {
                case model::OrderType::Move:
                    m_movement.issueMove(m_world, unit, order->targetPosition, false);
                    return;
                default:
                    // Other order payload fields are reserved for later command-queue slices.
                    break;
            }
        }
    }

    void GameLogicManager::handleQueuedOrders() {
        for (const auto& element : m_world.getElements()) {
            auto unit = std::dynamic_pointer_cast<model::Unit>(element);
            if (!unit ||
                unit->getAction() != model::ActionType::Idle ||
                !unit->hasQueuedOrders()) {
                continue;
            }

            issueNextQueuedOrder(*unit);
        }
    }

    void GameLogicManager::handleAttackCommand(const command::AttackCommand& cmd) {
        auto lock = m_world.acquireWriteLock();
        if (inputLocked()) return;

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
            if (cmd.append()) {
                queueMoveOrderForSelected(cmd.target());
                return;
            }
            m_movement.issueMove(m_world, m_selection.selected(), cmd.target());
            return;
        }

        if (cmd.append()) {
            if (!isOpposingTeam(attackerTeam, target->getTeamId())) {
                queueMoveOrderForSelected(target->getPosition());
            }
            return;
        }

        if (auto resource = std::dynamic_pointer_cast<model::ResourceNode>(target)) {
            clearSelectedUnitOrderQueues();
            issueGatherToResource(*resource);
            return;
        }

        // Right-click follows RTS convention: friendly target means move/approach,
        // opposing team target means attack.
        if (!isOpposingTeam(attackerTeam, target->getTeamId())) {
            m_movement.issueMove(m_world, m_selection.selected(), target->getPosition());
            return;
        }

        clearSelectedUnitOrderQueues();
        for (const auto& weak : m_selection.selected()) {
            if (auto attacker = weak.lock();
                attacker &&
                attacker.get() != target.get() &&
                attacker->getAction() != model::ActionType::Dead) {
                attacker->attack(target.get());
            }
        }
    }

    void GameLogicManager::handleAttackMoveCommand(const command::AttackMoveCommand& cmd) {
        auto lock = m_world.acquireWriteLock();
        if (inputLocked()) return;

        clearSelectedUnitOrderQueues();
        m_movement.issueAttackMove(m_world, m_selection.selected(), cmd.target());
    }

    void GameLogicManager::handlePatrolCommand(const command::PatrolCommand& cmd) {
        auto lock = m_world.acquireWriteLock();
        if (inputLocked()) return;

        clearSelectedUnitOrderQueues();
        m_movement.issuePatrol(m_world, m_selection.selected(), cmd.to());
    }

    void GameLogicManager::handleGatherCommand(const command::GatherCommand& cmd) {
        auto lock = m_world.acquireWriteLock();
        if (inputLocked()) return;

        if (!cmd.hasWorldTarget()) {
            return;
        }

        const auto target = findCommandTargetAt(cmd.target(), m_selection.selected());
        auto resource = std::dynamic_pointer_cast<model::ResourceNode>(target);
        if (!resource) {
            return;
        }

        clearSelectedUnitOrderQueues();
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
        if (inputLocked()) return;

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
        if (inputLocked()) return;

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
        if (inputLocked()) return;

        auto worker = firstSelectedWorker();
        if (!worker) {
            return;
        }

        worker->clearOrderQueue();
        m_movement.cancelQueuedPath(*worker);

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

    // =========================================================
    // Enemy AI & Victory / Defeat
    // =========================================================
    namespace {
        constexpr float kAiProduceInterval = 10.0f;  // refill barracks queue cadence
        constexpr float kAiWaveInterval = 35.0f;      // send a combined attack wave
    }

    std::shared_ptr<model::Building> GameLogicManager::findTownHall(int teamId) const {
        for (const auto& element : m_world.getElements()) {
            auto building = std::dynamic_pointer_cast<model::Building>(element);
            if (building &&
                building->buildingType() == model::BuildingType::TownHall &&
                building->getTeamId() == teamId &&
                building->getAction() != model::ActionType::Dead) {
                return building;
            }
        }
        return nullptr;
    }

    int GameLogicManager::countTownHalls(int teamId) const {
        int count = 0;
        for (const auto& element : m_world.getElements()) {
            auto building = std::dynamic_pointer_cast<model::Building>(element);
            if (building &&
                building->buildingType() == model::BuildingType::TownHall &&
                building->getTeamId() == teamId &&
                building->getAction() != model::ActionType::Dead) {
                ++count;
            }
        }
        return count;
    }

    void GameLogicManager::updateAI(float dt) {
        if (m_world.gameResult() != core::world::GameResult::InProgress) {
            return;
        }

        // Keep enemy barracks producing so waves have units to throw.
        m_aiProduceTimer += dt;
        if (m_aiProduceTimer >= kAiProduceInterval) {
            m_aiProduceTimer = 0.f;
            for (const auto& element : m_world.getElements()) {
                auto building = std::dynamic_pointer_cast<model::Building>(element);
                if (building &&
                    building->getTeamId() == model::TeamId::Enemy &&
                    building->buildingType() == model::BuildingType::Barracks &&
                    building->isComplete() &&
                    building->getAction() != model::ActionType::Dead &&
                    building->trainQueueSize() == 0) {
                    // AI trains for free (no cost check) to keep the slice self-driving.
                    building->trainUnit(::rts::UnitType::Warrior);
                }
            }
        }

        // Periodically launch every idle enemy combat unit at the player's town hall.
        m_aiWaveTimer += dt;
        if (m_aiWaveTimer >= kAiWaveInterval) {
            m_aiWaveTimer = 0.f;
            auto target = findTownHall(model::TeamId::Player);
            if (target) {
                for (const auto& element : m_world.getElements()) {
                    auto unit = std::dynamic_pointer_cast<model::Unit>(element);
                    if (unit &&
                        unit->getTeamId() == model::TeamId::Enemy &&
                        !unit->isWorker() &&
                        unit->getAction() == model::ActionType::Idle) {
                        m_movement.issueAttackMove(m_world, *unit, target->getPosition());
                    }
                }
            }
        }
    }

    void GameLogicManager::checkVictoryDefeat() {
        if (m_world.gameResult() != core::world::GameResult::InProgress) {
            return;
        }

        const int playerHalls = countTownHalls(model::TeamId::Player);
        const int enemyHalls = countTownHalls(model::TeamId::Enemy);

        if (playerHalls == 0) {
            m_world.setGameResult(core::world::GameResult::Defeat);
        } else if (enemyHalls == 0) {
            m_world.setGameResult(core::world::GameResult::Victory);
        }
    }

    bool GameLogicManager::inputLocked() const {
        return m_world.gameResult() != core::world::GameResult::InProgress;
    }
}
