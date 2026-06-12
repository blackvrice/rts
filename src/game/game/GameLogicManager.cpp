#include "game/game/GameLogicManager.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <vector>

#include <nlohmann/json.hpp>

#include <core/model/Unit.hpp>
#include <core/model/UnitType.hpp>
#include <core/model/Building.hpp>
#include <core/model/PlayerResourceState.hpp>
#include <core/model/ResourceNode.hpp>
#include <core/data/UnitStaticData.hpp>
#include <core/data/BuildingStaticData.hpp>
#include <core/data/ResourceStaticData.hpp>
#include <core/data/DataRegistry.hpp>
#include <core/data/DataPaths.hpp>
#include <core/map/MapData.hpp>
#include <core/tech/TechTreeValidator.hpp>
#include <core/world/GameWorld.hpp>
#include <core/world/WorldRuntimeServices.hpp>

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
        // Populate the initial match state. Extracted into setupInitialWorld so a
        // restart can rebuild the same starting position.
        {
            auto lock = m_world.acquireWriteLock();
            setupInitialWorld();
        }

        m_router.on<command::SelectCommand>([this](const command::SelectCommand &cmd) {
            if (!acceptPlayerCommand(cmd)) return;
            auto lock = m_world.acquireWriteLock();
            if (cmd.sameType()) {
                const auto a = cmd.area();
                const core::model::Vector2D point {
                    (a.left() + a.right()) * 0.5f,
                    (a.top() + a.bottom()) * 0.5f
                };
                m_selection.selectSameType(m_world, point, cmd.additive());
            } else {
                m_selection.selectInArea(m_world, cmd.area(), cmd.additive());
            }
            world::emitSound(m_world, world::SoundCue::Select, {}, 46.0f);
        });

        // Restart is accepted only once the match is decided (the result screen).
        m_router.on<command::RestartCommand>([this](const command::RestartCommand &) {
            if (m_world.gameResult() != core::world::GameResult::InProgress) {
                restartMatch();
            }
        });

        // Quick-save / quick-load to a fixed slot (F5 / F9).
        m_router.on<command::SaveGameCommand>([this](const command::SaveGameCommand &) {
            saveGame(quickSavePath());
        });
        m_router.on<command::LoadGameCommand>([this](const command::LoadGameCommand &) {
            loadGame(quickSavePath());
        });

        // Replay record/play (F6 / F7).
        m_router.on<command::ToggleRecordCommand>([this](const command::ToggleRecordCommand &) {
            toggleRecording();
        });
        m_router.on<command::PlayReplayCommand>([this](const command::PlayReplayCommand &) {
            startReplay();
        });

        m_router.on<command::MoveCommand>([this](const command::MoveCommand &cmd) {
            if (acceptPlayerCommand(cmd)) handleMoveCommand(cmd);
        });

        m_router.on<command::AttackCommand>([this](const command::AttackCommand &cmd) {
            if (acceptPlayerCommand(cmd)) handleAttackCommand(cmd);
        });

        m_router.on<command::AttackMoveCommand>([this](const command::AttackMoveCommand &cmd) {
            if (acceptPlayerCommand(cmd)) handleAttackMoveCommand(cmd);
        });

        m_router.on<command::GatherCommand>([this](const command::GatherCommand &cmd) {
            if (acceptPlayerCommand(cmd)) handleGatherCommand(cmd);
        });

        m_router.on<command::TrainUnitCommand>([this](const command::TrainUnitCommand &cmd) {
            if (acceptPlayerCommand(cmd)) handleTrainCommand(cmd);
        });

        m_router.on<command::CancelProductionCommand>([this](const command::CancelProductionCommand &cmd) {
            if (acceptPlayerCommand(cmd)) handleCancelProduction(cmd);
        });

        m_router.on<command::BuildCommand>([this](const command::BuildCommand &cmd) {
            if (acceptPlayerCommand(cmd)) handleBuildCommand(cmd);
        });

        m_router.on<command::StopCommand>([this](const command::StopCommand &cmd) {
            if (!acceptPlayerCommand(cmd)) return;
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

        m_router.on<command::HoldPositionCommand>([this](const command::HoldPositionCommand &cmd) {
            if (!acceptPlayerCommand(cmd)) return;
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
            if (acceptPlayerCommand(cmd)) handlePatrolCommand(cmd);
        });

        m_router.on<command::ControlGroupAddCommand>([this](const command::ControlGroupAddCommand &cmd) {
            if (!acceptPlayerCommand(cmd)) return;
            m_controlGroups.add(cmd.groupId(), m_selection.selected());
        });

        m_router.on<command::ControlGroupAssignCommand>([this](const command::ControlGroupAssignCommand &cmd) {
            if (!acceptPlayerCommand(cmd)) return;
            m_controlGroups.assign(cmd.groupId(), m_selection.selected());
        });

        m_router.on<command::ControlGroupSelectCommand>([this](const command::ControlGroupSelectCommand &cmd) {
            if (!acceptPlayerCommand(cmd)) return;
            auto lock = m_world.acquireWriteLock();
            m_selection.replaceSelected(m_controlGroups.select(cmd.groupId()));
        });
    }

    void GameLogicManager::update() {
        // Logic-level update hook for future AI and state transitions.
    }

    void GameLogicManager::tick(float dt) {
        // Playback: re-dispatch the recorded commands for this tick before the world
        // lock (each handler takes its own lock), matching live-command timing.
        if (m_replayMode == ReplayMode::Play) {
            applyReplayCommands(static_cast<std::uint64_t>(m_world.currentTick()));
        }
        auto lock = m_world.acquireWriteLock();
        m_world.advanceTick();
        world::updateRuntimeServices(m_world, dt);
        m_movement.update(m_world, dt, m_collision);
        world::rebuildSpatialIndex(m_world);
        m_world.updateProjectiles(dt);
        handleAttackRetargets();
        handleAttackMoveOrders();
        handlePatrolOrders();
        handleHoldPositionOrders();
        handleQueuedOrders();
        applyReadyResourceDeliveries();
        handleGatherRedirects();
        flushPendingSpawns();
        recomputeSupply();
        m_world.updateFog();
        captureFeedbackSnapshots();
        // Destroy EntityIds of units/buildings that died this tick so handles to
        // them stop validating (and recycled slots bump generation).
        m_world.pruneDeadEntities();
        updateAI(dt);
        checkVictoryDefeat();

        // Replay: record a periodic world-hash checkpoint while recording; compare
        // it while playing (logging divergence), and stop when the stream is spent.
        if (m_replayMode != ReplayMode::Off) {
            const auto t = static_cast<std::uint64_t>(m_world.currentTick());
            if (m_replayMode == ReplayMode::Record) {
                if (t % 30 == 0) {
                    m_replay.checkpoint(t, m_world.worldHash());
                }
            } else {  // Play
                if (const auto h = m_replay.hashForTick(t); h && *h != m_world.worldHash()) {
                    std::cerr << "[Replay] divergence at tick " << t << "\n";
                }
                if (t >= m_replay.lastTick()) {
                    m_replayMode = ReplayMode::Off;
                    std::cout << "[Replay] playback finished at tick " << t << "\n";
                }
            }
        }
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
                world::emitEffect(m_world, world::EffectType::ConstructionDust,
                                  cmd.target(), 34.0f, 0.35f);
            }
        }
        world::emitSound(m_world, world::SoundCue::MoveOrder, cmd.target(), 48.0f);
        m_movement.issueMove(m_world, m_selection.selected(), cmd.target());
    }

    std::shared_ptr<model::IGameElement> GameLogicManager::findCommandTargetAt(
        const model::Vector2D& target,
        const SelectionSystem::SelectedList& selected) const {
        std::shared_ptr<model::IGameElement> bestTarget;
        float bestDistanceSq = std::numeric_limits<float>::max();

        for (auto candidate : world::queryRadius(m_world, target, kAttackTargetPickRadius)) {
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
            world::emitSound(m_world, world::SoundCue::ResourceGather,
                             unit->getPosition(), 35.0f);
            world::emitEffect(m_world, world::EffectType::ResourceGather,
                              unit->getPosition(), 26.0f, 0.32f);
        }
    }

    std::shared_ptr<model::IGameElement> GameLogicManager::findClosestAttackMoveTarget(
        const model::Unit& unit) const {
        std::shared_ptr<model::IGameElement> bestTarget;
        float bestDistanceSq = std::numeric_limits<float>::max();
        const float acquireRadius = std::max(kAttackMoveAcquireRadius, unit.getAttackRange());
        const float acquireRadiusSq = acquireRadius * acquireRadius;

        for (auto candidate : world::queryRadius(m_world, unit.getPosition(), acquireRadius)) {
            if (!candidate || !unit.canAttackTarget(candidate.get())) {
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

    void GameLogicManager::handleAttackRetargets() {
        for (const auto& element : m_world.getElements()) {
            auto unit = std::dynamic_pointer_cast<model::Unit>(element);
            if (!unit ||
                unit->getAction() == model::ActionType::Dead ||
                !unit->needsAttackRetarget()) {
                continue;
            }

            // A queued command takes precedence over auto-retargeting: once the
            // direct attack target dies, fall through to the next queued order.
            if (unit->hasQueuedOrders()) {
                unit->clearAttackRetarget();
                continue;
            }

            if (auto target = findClosestAttackMoveTarget(*unit)) {
                unit->attack(target.get());
            } else {
                unit->clearAttackRetarget();
            }
        }
    }

    std::shared_ptr<model::IGameElement> GameLogicManager::findClosestPatrolTarget(
        const model::Unit& unit) const {
        std::shared_ptr<model::IGameElement> bestTarget;
        float bestDistanceSq = std::numeric_limits<float>::max();
        const float acquireRadius = std::max(kAttackMoveAcquireRadius, unit.getAttackRange());
        const float acquireRadiusSq = acquireRadius * acquireRadius;

        for (auto candidate : world::queryRadius(m_world, unit.getPosition(), acquireRadius)) {
            if (!candidate || !unit.canAttackTarget(candidate.get())) {
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

        for (auto candidate : world::queryRadius(m_world, unit.getPosition(), unit.getAttackRange())) {
            if (!candidate || !unit.canAttackTarget(candidate.get())) {
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

    void GameLogicManager::enqueueOrderForSelected(const model::UnitOrder& order, const bool workersOnly) {
        for (const auto& weak : m_selection.selected()) {
            auto unit = std::dynamic_pointer_cast<model::Unit>(weak.lock());
            if (!unit || unit->getAction() == model::ActionType::Dead) {
                continue;
            }
            if (workersOnly && !unit->isWorker()) {
                continue;
            }
            unit->enqueueOrder(order);
            // Kick off the first queued step right away when the unit is idle.
            if (unit->getAction() == model::ActionType::Idle) {
                issueNextQueuedOrder(*unit);
            }
        }
    }

    void GameLogicManager::issueNextQueuedOrder(model::Unit& unit) {
        // Pop orders until one can be started; dead/invalid targets are skipped so
        // the queue advances instead of stalling.
        while (auto order = unit.popNextOrder()) {
            switch (order->type) {
                case model::OrderType::Move:
                    m_movement.issueMove(m_world, unit, order->targetPosition, false);
                    return;
                case model::OrderType::AttackMove:
                    m_movement.issueAttackMove(m_world, unit, order->targetPosition);
                    return;
                case model::OrderType::Patrol:
                    m_movement.issuePatrol(m_world, unit, order->targetPosition);
                    return;
                case model::OrderType::Attack: {
                    auto target = m_world.resolve(order->targetEntityId);
                    if (target && target.get() != &unit &&
                        target->getAction() != model::ActionType::Dead) {
                        unit.attack(target.get());
                        return;
                    }
                    break;  // target gone: fall through to the next queued order
                }
                case model::OrderType::Gather: {
                    auto resource = std::dynamic_pointer_cast<model::ResourceNode>(
                        m_world.resolve(order->targetEntityId));
                    if (unit.isWorker() && resource && !resource->isDepleted()) {
                        if (auto dropOff = findClosestDropOffFor(unit)) {
                            unit.gather(resource.get(), dropOff.get());
                            return;
                        }
                    }
                    break;
                }
                default:
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

        // Prefer the EntityId the UI captured under the cursor, but only when it
        // resolves to a live resource or opposing unit near the click — otherwise
        // fall back to selection-aware positional resolution (unchanged UX).
        std::shared_ptr<model::IGameElement> target;
        if (core::ecs::isValid(cmd.targetEntityId())) {
            if (auto byId = m_world.resolve(cmd.targetEntityId())) {
                const bool validKind = std::dynamic_pointer_cast<model::ResourceNode>(byId) ||
                                       isOpposingTeam(attackerTeam, byId->getTeamId());
                const bool nearClick =
                    distanceSq(byId->getPosition(), cmd.target()) <= kAttackTargetPickRadiusSq;
                if (validKind && nearClick) {
                    target = std::move(byId);
                }
            }
        }
        if (!target) {
            target = findCommandTargetAt(cmd.target(), m_selection.selected());
        }
        if (!target) {
            if (cmd.append()) {
                queueMoveOrderForSelected(cmd.target());
                world::emitSound(m_world, world::SoundCue::MoveOrder, cmd.target(), 44.0f);
                return;
            }
            world::emitSound(m_world, world::SoundCue::MoveOrder, cmd.target(), 48.0f);
            m_movement.issueMove(m_world, m_selection.selected(), cmd.target());
            return;
        }

        // Shift-append: queue the smart action behind any existing orders so the
        // player can chain move/attack/gather steps (the order resolves its target
        // by EntityId when it runs).
        if (cmd.append()) {
            if (std::dynamic_pointer_cast<model::ResourceNode>(target)) {
                enqueueOrderForSelected(
                    model::UnitOrder { .type = model::OrderType::Gather,
                                       .targetEntityId = target->entityId() },
                    /*workersOnly=*/true);
                world::emitSound(m_world, world::SoundCue::ResourceGather,
                                 target->getPosition(), 42.0f);
            } else if (isOpposingTeam(attackerTeam, target->getTeamId())) {
                enqueueOrderForSelected(
                    model::UnitOrder { .type = model::OrderType::Attack,
                                       .targetEntityId = target->entityId() },
                    /*workersOnly=*/false);
                world::emitSound(m_world, world::SoundCue::AttackOrder,
                                 target->getPosition(), 56.0f);
            } else {
                queueMoveOrderForSelected(target->getPosition());
                world::emitSound(m_world, world::SoundCue::MoveOrder,
                                 target->getPosition(), 44.0f);
            }
            return;
        }

        if (auto resource = std::dynamic_pointer_cast<model::ResourceNode>(target)) {
            clearSelectedUnitOrderQueues();
            issueGatherToResource(*resource);
            world::emitSound(m_world, world::SoundCue::ResourceGather,
                             resource->getPosition(), 46.0f);
            return;
        }

        // Right-click follows RTS convention: friendly target means move/approach,
        // opposing team target means attack.
        if (!isOpposingTeam(attackerTeam, target->getTeamId())) {
            world::emitSound(m_world, world::SoundCue::MoveOrder,
                             target->getPosition(), 44.0f);
            m_movement.issueMove(m_world, m_selection.selected(), target->getPosition());
            return;
        }

        clearSelectedUnitOrderQueues();
        world::emitSound(m_world, world::SoundCue::AttackOrder,
                         target->getPosition(), 60.0f);
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
        world::emitSound(m_world, world::SoundCue::AttackOrder, cmd.target(), 54.0f);
        m_movement.issueAttackMove(m_world, m_selection.selected(), cmd.target());
    }

    void GameLogicManager::handlePatrolCommand(const command::PatrolCommand& cmd) {
        auto lock = m_world.acquireWriteLock();
        if (inputLocked()) return;

        clearSelectedUnitOrderQueues();
        world::emitSound(m_world, world::SoundCue::MoveOrder, cmd.to(), 42.0f);
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
        world::emitSound(m_world, world::SoundCue::ResourceGather,
                         resource->getPosition(), 46.0f);
    }

    // =========================================================
    // Production
    // =========================================================
    ::rts::UnitType GameLogicManager::defaultUnitFor(model::BuildingType type) {
        // The first entry of the building's produces list is its default unit.
        const auto& produces = core::data::DataRegistry::global().building(type).produces;
        return produces.empty() ? ::rts::UnitType::Warrior : produces.front();
    }

    core::tech::TechState GameLogicManager::buildTechState(const int teamId) const {
        core::tech::TechState state;
        for (const auto& element : m_world.getElements()) {
            auto building = std::dynamic_pointer_cast<model::Building>(element);
            if (building && building->getTeamId() == teamId &&
                building->isComplete() &&
                building->getAction() != model::ActionType::Dead) {
                state.completedBuildings.insert(building->buildingType());
            }
        }
        // researchedUpgrades stays empty: no upgrade/research content exists yet.
        return state;
    }

    bool GameLogicManager::hasBuildingRequirements(
        const int teamId, const data::BuildingStaticData& data) const {
        return core::tech::TechTreeValidator::canBuild(
            buildTechState(teamId), data.buildingType).ok;
    }

    void GameLogicManager::recomputeSupply() {
        // Recompute population economy from live state each tick so it self-corrects
        // for deaths, cancels and spawns (TeamId indexes 0=Neutral, 1=Player, 2=Enemy):
        //   capacity = sum of providesSupply over completed buildings
        //   foodUsed = food of live units + food of units still in training queues
        //   army     = count of live combat (non-worker) units
        int capacity[3] = { 0, 0, 0 };
        int used[3]     = { 0, 0, 0 };
        int army[3]     = { 0, 0, 0 };
        const auto& registry = core::data::DataRegistry::global();

        for (const auto& element : m_world.getElements()) {
            if (auto building = std::dynamic_pointer_cast<model::Building>(element)) {
                if (building->getAction() == model::ActionType::Dead) continue;
                const int team = building->getTeamId();
                if (team < 0 || team > 2) continue;
                if (building->isComplete()) {
                    capacity[team] += registry.building(building->buildingType()).providesSupply;
                }
                // Units still in production reserve their food before they spawn.
                for (int i = 0; i < building->trainQueueSize(); ++i) {
                    used[team] += registry.unit(building->trainQueueAt(i)).foodCost;
                }
            } else if (auto unit = std::dynamic_pointer_cast<model::Unit>(element)) {
                if (unit->getAction() == model::ActionType::Dead) continue;
                const int team = unit->getTeamId();
                if (team < 0 || team > 2) continue;
                used[team] += registry.unit(unit->unitType()).foodCost;
                if (!unit->isWorker()) ++army[team];
            }
        }

        for (const int teamId : { model::TeamId::Player, model::TeamId::Enemy }) {
            auto resources = m_world.playerResources(teamId);
            resources.foodCapacity = capacity[teamId];
            resources.foodUsed = used[teamId];
            resources.army = army[teamId];
            m_world.setPlayerResources(teamId, resources);
            logResourceChange(teamId, resources);
        }
        m_resourceLogReady = true;
    }

    void GameLogicManager::logResourceChange(
        const int teamId, const core::model::PlayerResourceState& current) {
        if (teamId < 0 || teamId > 2) return;
        const auto& prev = m_lastResourceLog[teamId];
        const bool changed = !m_resourceLogReady ||
            prev.gold != current.gold || prev.wood != current.wood ||
            prev.foodUsed != current.foodUsed || prev.foodCapacity != current.foodCapacity ||
            prev.army != current.army;
        if (!changed) return;

        if (m_resourceLogReady) {
            const auto delta = [](const int now, const int before) {
                const int d = now - before;
                return (d >= 0 ? "+" : "") + std::to_string(d);
            };
            std::cerr << "[Resource] team " << teamId
                      << " gold=" << current.gold << "(" << delta(current.gold, prev.gold) << ")"
                      << " wood=" << current.wood << "(" << delta(current.wood, prev.wood) << ")"
                      << " food=" << current.foodUsed << "/" << current.foodCapacity
                      << " army=" << current.army << "(" << delta(current.army, prev.army) << ")\n";
        }
        m_lastResourceLog[teamId] = current;
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

    std::optional<model::Vector2D> GameLogicManager::findFreeSpawnPosition(
        const model::Vector2D& anchor,
        const std::optional<model::Vector2D>& prefer) const {
        const auto& tf = m_world.gridTransform();
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
                    if (m_world.isTileBlocked(gx, gy) || m_world.isCellOccupied(gx, gy)) {
                        continue;
                    }
                    const auto world = tf.gridToWorldCenter(path::GridPos{ gx, gy });
                    if (!prefer) {
                        return world;  // no preference: first free tile is fine
                    }
                    const float dSq = distanceSq(world, *prefer);
                    if (dSq < bestDistSq) {
                        bestDistSq = dSq;
                        best = world;
                    }
                }
            }
            if (best) {
                return best;  // closest-to-rally free tile in the nearest occupied ring
            }
        }
        return std::nullopt;
    }

    bool GameLogicManager::isEnemyNear(const model::Vector2D& point, const int team) const {
        for (auto candidate : world::queryRadius(m_world, point, kAttackTargetPickRadius)) {
            if (!candidate ||
                candidate->getAction() == model::ActionType::Dead ||
                !isOpposingTeam(team, candidate->getTeamId())) {
                continue;
            }
            if (distanceSq(candidate->getPosition(), point) <= kAttackTargetPickRadiusSq) {
                return true;
            }
        }
        return false;
    }

    void GameLogicManager::flushPendingSpawns() {
        if (m_pendingSpawns.empty()) {
            return;
        }

        // Move out first so spawned-unit side effects can't grow the buffer we iterate.
        std::vector<PendingSpawn> spawns;
        spawns.swap(m_pendingSpawns);

        for (const auto& spawn : spawns) {
            const std::optional<model::Vector2D> prefer =
                spawn.hasRally ? std::optional{ spawn.rally } : std::nullopt;
            const auto position = findFreeSpawnPosition(spawn.anchor, prefer);
            if (!position) {
                // No room around the building yet — hold the unit and retry next tick.
                m_pendingSpawns.push_back(spawn);
                continue;
            }

            auto unit = std::make_shared<model::Unit>(spawn.type);
            unit->setPosition(*position);
            unit->setTeamId(spawn.team);
            m_world.addElement(unit);
            world::emitSound(m_world, world::SoundCue::ProductionComplete,
                             *position, spawn.team == model::TeamId::Player ? 62.0f : 34.0f);
            world::emitEffect(m_world, world::EffectType::ConstructionDust,
                              *position, 42.0f, 0.45f);

            if (spawn.hasRally) {
                // A rally point on/near an enemy means attack-move there instead of
                // a passive move, so rallied units engage on arrival.
                if (isEnemyNear(spawn.rally, spawn.team)) {
                    m_movement.issueAttackMove(m_world, *unit, spawn.rally);
                } else {
                    unit->moveTo(spawn.rally);
                }
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

        // Tech gate: the selected building enforces "produced here"; this also checks
        // the unit's own prerequisites (e.g. a future tech building).
        if (!core::tech::TechTreeValidator::canProduce(
                buildTechState(building->getTeamId()), unitType).ok) {
            return;
        }

        if (building->trainQueueSize() >= model::Building::kMaxTrainQueue) {
            return;
        }

        const auto staticData = core::data::unitStaticDataFor(unitType);
        const auto cost = staticData.cost();

        auto resources = m_world.playerResources(building->getTeamId());
        if (!resources.canAfford(cost)) {
            world::emitSound(m_world, world::SoundCue::ResourceShortage,
                             building->getPosition(), 70.0f);
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

        // Reject the build when a prerequisite building is missing for the team.
        if (!hasBuildingRequirements(worker->getTeamId(), data)) {
            return;
        }

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
            world::emitSound(m_world, world::SoundCue::ResourceShortage,
                             worker->getPosition(), 70.0f);
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
        world::emitSound(m_world, world::SoundCue::ConstructionComplete,
                         buildingPos, 38.0f);
        world::emitEffect(m_world, world::EffectType::ConstructionDust,
                          buildingPos, 72.0f, 0.65f);

        resources.pay(data.cost());
        m_world.setPlayerResources(worker->getTeamId(), resources);

        worker->buildAt(site.get());
    }

    // =========================================================
    // Match lifecycle
    // =========================================================
    void GameLogicManager::setupInitialWorld() {
        // Scenario comes from data/maps/skirmish.json (falls back to a built-in
        // default), so the starting layout is editable without recompiling.
        const auto map = core::map::loadMap(
            std::string(core::data::DataRoot) + "/maps/skirmish.json");

        m_world.initTileMap(map.width, map.height, map.tileSize);
        for (const auto& tile : map.blockedTiles) {
            m_world.setTileBlocked(tile.x, tile.y, true);
        }

        const auto makeResources = [](int gold, int wood) {
            core::model::PlayerResourceState r {};
            r.gold = gold;
            r.wood = wood;
            r.foodUsed = 0;
            r.foodCapacity = 20;
            r.army = 0;
            return r;
        };
        m_world.setPlayerResources(core::model::TeamId::Player,
            makeResources(map.playerGold, map.playerWood));
        m_world.setPlayerResources(core::model::TeamId::Enemy,
            makeResources(map.enemyGold, map.enemyWood));

        for (const auto& b : map.buildings) {
            auto building = std::make_shared<core::model::Building>(b.type, b.position, b.teamId);
            registerBuildingSpawn(*building);
            m_world.addElement(building);
        }

        for (const auto& u : map.units) {
            auto unit = std::make_shared<core::model::Unit>(u.type);
            unit->setPosition(u.position);
            unit->setTeamId(u.teamId);
            m_world.addElement(unit);
        }

        for (const auto& r : map.resources) {
            const auto data = core::data::resourceStaticDataFor(r.type);
            auto node = std::make_shared<core::model::ResourceNode>(
                r.position, data.resourceType, data.initialAmount,
                data.gatherAmountPerTrip, data.gatherDurationSeconds, data.maxGatherers);
            m_world.addElement(node);
        }
    }

    void GameLogicManager::restartMatch() {
        auto lock = m_world.acquireWriteLock();
        m_world.resetForNewMatch();
        m_selection.clear();
        m_movement.reset();
        m_pendingSpawns.clear();
        m_aiProduceTimer = 0.f;
        m_aiGatherTimer = 0.f;
        m_aiWaveTimer = 0.f;
        m_aiDefenseTimer = 0.f;
        m_aiState = AiBuildOrderState::Opening;
        m_lastFeedbackAiState = AiBuildOrderState::Opening;
        m_feedbackSnapshots.clear();
        world::resetRuntimeServices(m_world);
        setupInitialWorld();
        world::rebuildSpatialIndex(m_world);
    }

    std::string GameLogicManager::quickSavePath() {
        return std::string(core::data::DataRoot) + "/saves/quicksave.json";
    }

    bool GameLogicManager::saveGame(const std::string& path) {
        using json = nlohmann::json;
        json doc;
        {
            auto lock = m_world.acquireReadLock();
            doc["tick"] = static_cast<std::uint64_t>(m_world.currentTick());

            json players = json::array();
            for (const int team : { core::model::TeamId::Player, core::model::TeamId::Enemy }) {
                const auto& r = m_world.playerResources(team);
                players.push_back({
                    { "team", team }, { "gold", r.gold }, { "wood", r.wood },
                    { "foodUsed", r.foodUsed }, { "foodCapacity", r.foodCapacity }, { "army", r.army }
                });
            }
            doc["players"] = players;

            json units = json::array();
            json buildings = json::array();
            json resources = json::array();
            for (const auto& el : m_world.getElements()) {
                auto ge = std::dynamic_pointer_cast<core::model::IGameElement>(el);
                if (!ge || ge->getAction() == core::model::ActionType::Dead) {
                    continue;
                }
                const auto pos = ge->getPosition();
                if (auto u = std::dynamic_pointer_cast<core::model::Unit>(el)) {
                    units.push_back({
                        { "type", static_cast<int>(u->unitType()) }, { "team", u->getTeamId() },
                        { "x", pos.x }, { "y", pos.y }, { "hp", u->getHp() }
                    });
                } else if (auto b = std::dynamic_pointer_cast<core::model::Building>(el)) {
                    json queue = json::array();
                    for (int i = 0; i < b->trainQueueSize(); ++i) {
                        queue.push_back(static_cast<int>(b->trainQueueAt(i)));
                    }
                    buildings.push_back({
                        { "type", static_cast<int>(b->buildingType()) }, { "team", b->getTeamId() },
                        { "x", pos.x }, { "y", pos.y }, { "hp", b->getHp() },
                        { "completed", b->isComplete() }, { "queue", queue }
                    });
                } else if (auto rn = std::dynamic_pointer_cast<core::model::ResourceNode>(el)) {
                    resources.push_back({
                        { "type", static_cast<int>(rn->type()) },
                        { "x", pos.x }, { "y", pos.y }, { "remaining", rn->remaining() }
                    });
                }
            }
            doc["units"] = units;
            doc["buildings"] = buildings;
            doc["resources"] = resources;
        }

        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
        std::ofstream out(path);
        if (!out) {
            std::cerr << "[SaveGame] cannot write " << path << "\n";
            return false;
        }
        out << doc.dump(2);
        std::cout << "[SaveGame] saved to " << path << "\n";
        return true;
    }

    bool GameLogicManager::loadGame(const std::string& path) {
        using json = nlohmann::json;
        std::ifstream in(path);
        if (!in) {
            std::cerr << "[LoadGame] cannot open " << path << "\n";
            return false;
        }
        json doc;
        try {
            in >> doc;
        } catch (const std::exception& e) {
            std::cerr << "[LoadGame] parse error: " << e.what() << "\n";
            return false;
        }

        auto lock = m_world.acquireWriteLock();
        // Mirror restartMatch's teardown, then rebuild from the snapshot instead of
        // the map's initial layout.
        m_world.resetForNewMatch();
        m_selection.clear();
        m_movement.reset();
        m_pendingSpawns.clear();
        m_aiProduceTimer = 0.f;
        m_aiGatherTimer = 0.f;
        m_aiWaveTimer = 0.f;
        m_aiDefenseTimer = 0.f;
        m_aiState = AiBuildOrderState::Opening;
        m_lastFeedbackAiState = AiBuildOrderState::Opening;
        m_feedbackSnapshots.clear();
        world::resetRuntimeServices(m_world);

        for (const auto& p : doc.value("players", json::array())) {
            core::model::PlayerResourceState r {};
            r.gold = p.value("gold", r.gold);
            r.wood = p.value("wood", r.wood);
            r.foodUsed = p.value("foodUsed", 0);
            r.foodCapacity = p.value("foodCapacity", 0);
            r.army = p.value("army", 0);
            m_world.setPlayerResources(p.value("team", core::model::TeamId::Neutral), r);
        }

        for (const auto& b : doc.value("buildings", json::array())) {
            const auto type = static_cast<core::model::BuildingType>(b.value("type", 0));
            const core::model::Vector2D pos { b.value("x", 0.0f), b.value("y", 0.0f) };
            const int team = b.value("team", core::model::TeamId::Neutral);
            auto building = std::make_shared<core::model::Building>(type, pos, team);
            registerBuildingSpawn(*building);
            building->setHp(b.value("hp", building->getMaxHp()));
            if (!b.value("completed", true)) {
                const auto data = core::data::buildingStaticDataFor(type);
                building->beginConstruction(data.buildTimeSeconds, building->getHp());
            }
            m_world.addElement(building);
            for (const auto& q : b.value("queue", json::array())) {
                building->trainUnit(static_cast<::rts::UnitType>(q.get<int>()));
            }
        }

        for (const auto& u : doc.value("units", json::array())) {
            auto unit = std::make_shared<core::model::Unit>(
                static_cast<::rts::UnitType>(u.value("type", 0)));
            unit->setPosition({ u.value("x", 0.0f), u.value("y", 0.0f) });
            unit->setTeamId(u.value("team", core::model::TeamId::Neutral));
            unit->setHp(u.value("hp", unit->getMaxHp()));
            m_world.addElement(unit);
        }

        for (const auto& r : doc.value("resources", json::array())) {
            const auto data = core::data::resourceStaticDataFor(
                static_cast<core::model::ResourceNode::ResourceType>(r.value("type", 0)));
            const core::model::Vector2D pos { r.value("x", 0.0f), r.value("y", 0.0f) };
            auto node = std::make_shared<core::model::ResourceNode>(
                pos, data.resourceType, data.initialAmount,
                data.gatherAmountPerTrip, data.gatherDurationSeconds, data.maxGatherers);
            node->setRemaining(r.value("remaining", data.initialAmount));
            m_world.addElement(node);
        }

        m_world.setCurrentTick(static_cast<core::sim::TickCount>(doc.value("tick", 0ull)));
        world::rebuildSpatialIndex(m_world);
        std::cout << "[LoadGame] loaded from " << path << "\n";
        return true;
    }

    // =========================================================
    // Replay
    // =========================================================
    std::string GameLogicManager::replayPath() {
        return std::string(core::data::DataRoot) + "/saves/replay.json";
    }

    void GameLogicManager::toggleRecording() {
        if (m_replayMode == ReplayMode::Record) {
            m_replay.save(replayPath());
            m_replayMode = ReplayMode::Off;
            std::cout << "[Replay] recording stopped and saved\n";
            return;
        }
        // Begin from a known initial state so the stream replays deterministically.
        restartMatch();
        m_replay.clear();
        m_replay.setMapPath(std::string(core::data::DataRoot) + "/maps/skirmish.json");
        m_replayMode = ReplayMode::Record;
        std::cout << "[Replay] recording started\n";
    }

    void GameLogicManager::startReplay() {
        if (!m_replay.load(replayPath())) {
            return;
        }
        restartMatch();  // rewind to the initial state the replay was recorded from
        m_replayMode = ReplayMode::Play;
        std::cout << "[Replay] playback started (" << m_replay.lastTick() << " ticks)\n";
    }

    bool GameLogicManager::acceptPlayerCommand(const command::LogicCommand& c) {
        if (m_replayMode == ReplayMode::Play && !m_applyingReplay) {
            return false;  // live input is ignored while a replay plays
        }
        if (m_replayMode == ReplayMode::Record && !m_applyingReplay) {
            if (auto j = core::replay::serializeLogicCommand(c)) {
                m_replay.record(static_cast<std::uint64_t>(m_world.currentTick()), *j);
            }
        }
        return true;
    }

    void GameLogicManager::applyReplayCommands(const std::uint64_t tick) {
        const auto cmds = m_replay.commandsForTick(tick);
        if (cmds.empty()) {
            return;
        }
        // Dispatched outside the tick's world lock (each handler takes its own lock),
        // matching how live commands are drained before tick().
        m_applyingReplay = true;
        for (const auto* e : cmds) {
            if (auto cmd = core::replay::deserializeLogicCommand(e->cmd)) {
                m_router.dispatch(*cmd);
            }
        }
        m_applyingReplay = false;
    }

    // =========================================================
    // Enemy AI & Victory / Defeat
    // =========================================================
    namespace {
        constexpr float kAiProduceInterval = 5.0f;   // train workers/warriors cadence
        constexpr float kAiGatherInterval = 3.0f;     // assign idle workers cadence
        constexpr float kAiDefenseInterval = 1.0f;    // tactical threat scan cadence
        constexpr float kAiWaveInterval = 45.0f;       // send a wave even if undersized
        constexpr int   kAiMaxWorkers = 6;             // economy worker cap
        constexpr int   kAiWaveArmySize = 6;           // launch once this many idle soldiers mass
        constexpr float kAiDefenseRadius = 420.0f;
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

    std::shared_ptr<model::Building> GameLogicManager::findBarracks(
        const int teamId, const bool requireComplete) const {
        for (const auto& element : m_world.getElements()) {
            auto building = std::dynamic_pointer_cast<model::Building>(element);
            if (building &&
                building->buildingType() == model::BuildingType::Barracks &&
                building->getTeamId() == teamId &&
                building->getAction() != model::ActionType::Dead &&
                (!requireComplete || building->isComplete())) {
                return building;
            }
        }
        return nullptr;
    }

    std::shared_ptr<model::Unit> GameLogicManager::findIdleWorker(const int teamId) const {
        for (const auto& element : m_world.getElements()) {
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

    int GameLogicManager::countCombatUnits(const int teamId, const bool idleOnly) const {
        int count = 0;
        for (const auto& element : m_world.getElements()) {
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

    model::Vector2D GameLogicManager::aiRallyPoint() const {
        const auto enemyHall = findTownHall(model::TeamId::Enemy);
        const auto playerHall = findTownHall(model::TeamId::Player);
        if (enemyHall && playerHall) {
            const auto a = enemyHall->getPosition();
            const auto b = playerHall->getPosition();
            return { a.x * 0.65f + b.x * 0.35f, a.y * 0.65f + b.y * 0.35f };
        }
        return enemyHall ? enemyHall->getPosition() : model::Vector2D{ 0.0f, 0.0f };
    }

    std::optional<model::Vector2D> GameLogicManager::findBuildSiteNear(
        const model::Vector2D& center, const data::BuildingStaticData& data) const {
        const auto& tf = m_world.gridTransform();
        const auto centerCell = tf.worldToGrid(center);
        for (int radius = 5; radius <= 14; ++radius) {
            for (int dy = -radius; dy <= radius; ++dy) {
                for (int dx = -radius; dx <= radius; ++dx) {
                    if (std::max(std::abs(dx), std::abs(dy)) != radius) {
                        continue;
                    }
                    const int originX = centerCell.x + dx - data.footprintWidth / 2;
                    const int originY = centerCell.y + dy - data.footprintHeight / 2;
                    if (!canPlaceBuilding(originX, originY, data.footprintWidth, data.footprintHeight)) {
                        continue;
                    }
                    const float halfW = data.footprintWidth * tf.tileSize * 0.5f;
                    const float halfH = data.footprintHeight * tf.tileSize * 0.5f;
                    const model::Vector2D originCenter =
                        tf.gridToWorldCenter(path::GridPos{ originX, originY });
                    return model::Vector2D {
                        originCenter.x - tf.tileSize * 0.5f + halfW,
                        originCenter.y - tf.tileSize * 0.5f + halfH
                    };
                }
            }
        }
        return std::nullopt;
    }

    bool GameLogicManager::tryStartAiBarracks() {
        if (findBarracks(model::TeamId::Enemy, false)) {
            return false;
        }
        auto worker = findIdleWorker(model::TeamId::Enemy);
        auto townHall = findTownHall(model::TeamId::Enemy);
        if (!worker || !townHall) {
            return false;
        }

        const auto data = core::data::buildingStaticDataFor(model::BuildingType::Barracks);
        if (!hasBuildingRequirements(model::TeamId::Enemy, data)) {
            return false;
        }

        auto resources = m_world.playerResources(model::TeamId::Enemy);
        if (!resources.canAfford(data.cost())) {
            return false;
        }

        const auto sitePos = findBuildSiteNear(townHall->getPosition(), data);
        if (!sitePos) {
            return false;
        }

        auto site = std::make_shared<model::Building>(
            model::BuildingType::Barracks, *sitePos, model::TeamId::Enemy);
        site->beginConstruction(data.buildTimeSeconds, data.maxHp * 0.1f);
        site->setRallyPoint(aiRallyPoint());
        registerBuildingSpawn(*site);
        m_world.addElement(site);
        resources.pay(data.cost());
        m_world.setPlayerResources(model::TeamId::Enemy, resources);
        worker->buildAt(site.get());
        world::emitEffect(m_world, world::EffectType::ConstructionDust, *sitePos, 72.0f, 0.65f);
        return true;
    }

    void GameLogicManager::emitStateFeedback() {
        if (m_aiState == m_lastFeedbackAiState) {
            return;
        }
        m_lastFeedbackAiState = m_aiState;
        if (m_aiState == AiBuildOrderState::Attack) {
            world::emitSound(m_world, world::SoundCue::AttackOrder, aiRallyPoint(), 35.0f);
        }
    }

    void GameLogicManager::updateAI(float dt) {
        if (m_world.gameResult() != core::world::GameResult::InProgress) {
            return;
        }

        updateAiBuildOrder();
        updateAiProduction(dt);
        updateAiWorkers(dt);
        updateAiDefense(dt);
        updateAiWaves(dt);
    }

    void GameLogicManager::updateAiBuildOrder() {
        const auto barracks = findBarracks(model::TeamId::Enemy, false);
        const int combat = countCombatUnits(model::TeamId::Enemy, false);
        if (!barracks) {
            m_aiState = combat > 0 ? AiBuildOrderState::Rebuild : AiBuildOrderState::BuildBarracks;
            tryStartAiBarracks();
        } else if (combat >= kAiWaveArmySize) {
            m_aiState = AiBuildOrderState::Attack;
        } else if (combat > 0 || findBarracks(model::TeamId::Enemy, true)) {
            m_aiState = AiBuildOrderState::ProduceArmy;
        } else {
            m_aiState = AiBuildOrderState::Gather;
        }
        emitStateFeedback();
    }

    void GameLogicManager::updateAiProduction(const float dt) {
        m_aiProduceTimer += dt;
        if (m_aiProduceTimer < kAiProduceInterval) {
            return;
        }
        m_aiProduceTimer = 0.f;

        auto resources = m_world.playerResources(model::TeamId::Enemy);

        int workerCount = 0;
        for (const auto& element : m_world.getElements()) {
            auto unit = std::dynamic_pointer_cast<model::Unit>(element);
            if (unit && unit->getTeamId() == model::TeamId::Enemy &&
                unit->isWorker() && unit->getAction() != model::ActionType::Dead) {
                ++workerCount;
            }
        }

        // Train from idle production buildings, paying from the enemy pool. Workers
        // grow the economy up to a cap; barracks keep warriors coming for waves.
        for (const auto& element : m_world.getElements()) {
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
                building->setRallyPoint(aiRallyPoint());
                unitType = defaultUnitFor(building->buildingType());
            }

            const auto cost = core::data::unitStaticDataFor(unitType).cost();
            if (resources.canAfford(cost) && building->trainUnit(unitType)) {
                resources.pay(cost);
                if (unitType == ::rts::UnitType::Worker) {
                    ++workerCount;
                }
            }
        }

        m_world.setPlayerResources(model::TeamId::Enemy, resources);
    }

    void GameLogicManager::updateAiWorkers(const float dt) {
        m_aiGatherTimer += dt;
        if (m_aiGatherTimer < kAiGatherInterval) {
            return;
        }
        m_aiGatherTimer = 0.f;

        for (const auto& element : m_world.getElements()) {
            auto worker = std::dynamic_pointer_cast<model::Unit>(element);
            if (!worker ||
                worker->getTeamId() != model::TeamId::Enemy ||
                !worker->isWorker() ||
                worker->getAction() != model::ActionType::Idle) {
                continue;
            }

            auto resource = findClosestAvailableResource(
                m_world.playerResources(model::TeamId::Enemy).wood < 120
                    ? model::ResourceNode::ResourceType::Wood
                    : model::ResourceNode::ResourceType::Gold,
                *worker);
            if (!resource) {
                resource = findClosestAvailableResource(
                    model::ResourceNode::ResourceType::Wood, *worker);
            }
            if (!resource) {
                continue;
            }

            auto dropOff = findClosestDropOffFor(*worker);
            if (dropOff) {
                worker->gather(resource.get(), dropOff.get());
            }
        }
    }

    void GameLogicManager::updateAiDefense(const float dt) {
        m_aiDefenseTimer += dt;
        if (m_aiDefenseTimer < kAiDefenseInterval) {
            return;
        }
        m_aiDefenseTimer = 0.0f;

        std::optional<model::Vector2D> threatPos;
        for (const auto& element : m_world.getElements()) {
            auto building = std::dynamic_pointer_cast<model::Building>(element);
            if (!building ||
                building->getTeamId() != model::TeamId::Enemy ||
                building->getAction() == model::ActionType::Dead) {
                continue;
            }
            for (const auto& nearby : world::queryRadius(
                     m_world, building->getPosition(), kAiDefenseRadius)) {
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

        for (const auto& element : m_world.getElements()) {
            auto unit = std::dynamic_pointer_cast<model::Unit>(element);
            if (unit &&
                unit->getTeamId() == model::TeamId::Enemy &&
                !unit->isWorker() &&
                unit->getAction() == model::ActionType::Idle) {
                m_movement.issueAttackMove(m_world, *unit, *threatPos);
            }
        }
        world::emitSound(m_world, world::SoundCue::AttackOrder, *threatPos, 34.0f);
    }

    void GameLogicManager::updateAiWaves(const float dt) {
        m_aiWaveTimer += dt;

        int idleSoldiers = 0;
        for (const auto& element : m_world.getElements()) {
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
        const bool timedOut = m_aiWaveTimer >= kAiWaveInterval;
        if ((!massReady && !timedOut) || idleSoldiers == 0) {
            if (idleSoldiers > 0) {
                const auto rally = aiRallyPoint();
                for (const auto& element : m_world.getElements()) {
                    auto unit = std::dynamic_pointer_cast<model::Unit>(element);
                    if (unit &&
                        unit->getTeamId() == model::TeamId::Enemy &&
                        !unit->isWorker() &&
                        unit->getAction() == model::ActionType::Idle &&
                        distanceSq(unit->getPosition(), rally) > 80.0f * 80.0f) {
                        m_movement.issueMove(m_world, *unit, rally, false);
                    }
                }
            }
            return;
        }
        m_aiWaveTimer = 0.f;

        auto target = findTownHall(model::TeamId::Player);
        if (!target) {
            return;
        }
        for (const auto& element : m_world.getElements()) {
            auto unit = std::dynamic_pointer_cast<model::Unit>(element);
            if (unit &&
                unit->getTeamId() == model::TeamId::Enemy &&
                !unit->isWorker() &&
                unit->getAction() == model::ActionType::Idle) {
                m_movement.issueAttackMove(m_world, *unit, target->getPosition());
            }
        }
        world::emitSound(m_world, world::SoundCue::AttackOrder, target->getPosition(), 42.0f);
    }

    void GameLogicManager::checkVictoryDefeat() {
        if (m_world.gameResult() != core::world::GameResult::InProgress) {
            return;
        }

        const int playerHalls = countTownHalls(model::TeamId::Player);
        const int enemyHalls = countTownHalls(model::TeamId::Enemy);

        if (playerHalls == 0) {
            m_world.setGameResult(core::world::GameResult::Defeat);
            world::emitSound(m_world, world::SoundCue::Defeat, {}, 80.0f);
        } else if (enemyHalls == 0) {
            m_world.setGameResult(core::world::GameResult::Victory);
            world::emitSound(m_world, world::SoundCue::Victory, {}, 80.0f);
        }
    }

    void GameLogicManager::captureFeedbackSnapshots() {
        std::unordered_map<std::uint32_t, ElementSnapshot> next;
        for (const auto& element : m_world.getElements()) {
            auto gameElement = std::dynamic_pointer_cast<model::IGameElement>(element);
            if (!gameElement) {
                continue;
            }
            float hp = 0.0f;
            bool complete = false;
            if (const auto unit = std::dynamic_pointer_cast<model::Unit>(element)) {
                hp = unit->getHp();
            } else if (const auto building = std::dynamic_pointer_cast<model::Building>(element)) {
                hp = building->getHp();
                complete = building->isComplete();
            } else if (const auto resource = std::dynamic_pointer_cast<model::ResourceNode>(element)) {
                hp = static_cast<float>(resource->remaining());
            }

            const auto id = gameElement->entityId().index;
            const auto action = gameElement->getAction();
            if (const auto it = m_feedbackSnapshots.find(id); it != m_feedbackSnapshots.end()) {
                const auto& prev = it->second;
                if (hp < prev.hp && action != model::ActionType::Dead) {
                    world::emitSound(m_world, world::SoundCue::Hit,
                                     gameElement->getPosition(), 44.0f);
                    world::emitEffect(m_world, world::EffectType::HitSpark,
                                      gameElement->getPosition(), 34.0f, 0.28f);
                }
                if (prev.action != model::ActionType::Dead &&
                    action == model::ActionType::Dead) {
                    const auto pos = gameElement->getPosition();
                    world::emitSound(m_world, world::SoundCue::Death, pos, 58.0f);
                    world::emitEffect(m_world, world::EffectType::DeathBurst, pos, 52.0f, 0.6f);
                    world::emitEffect(m_world, world::EffectType::BloodDecal, pos, 24.0f, 6.0f, 0x4A050566u);
                }
                if (!prev.complete && complete) {
                    world::emitSound(m_world, world::SoundCue::ConstructionComplete,
                                     gameElement->getPosition(), 62.0f);
                    world::emitEffect(m_world, world::EffectType::ConstructionDust,
                                      gameElement->getPosition(), 82.0f, 0.75f);
                }
            }
            next[id] = ElementSnapshot {
                .hp = hp,
                .complete = complete,
                .action = action
            };
        }
        m_feedbackSnapshots.swap(next);
    }

    bool GameLogicManager::inputLocked() const {
        return m_world.gameResult() != core::world::GameResult::InProgress;
    }
}
