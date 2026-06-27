#include "game/game/GameLogicManager.hpp"

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/app/SessionContext.hpp"
#include "core/command/LogicCommand.hpp"

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
    // Upper bound on a target's body radius (largest building footprint half-
    // diagonal) used to widen acquisition queries before per-target edge gating.
    constexpr float kMaxTargetHitRadius = 200.0f;

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

    std::uint64_t entityKey(const rts::core::ecs::EntityId id) {
        return (static_cast<std::uint64_t>(id.index) << 32) | id.generation;
    }

    nlohmann::json entityRef(const rts::core::ecs::EntityId id) {
        return nlohmann::json { { "index", id.index }, { "generation", id.generation } };
    }

    rts::core::ecs::EntityId entityIdFromJson(const nlohmann::json& j) {
        return rts::core::ecs::EntityId {
            j.value("index", rts::core::ecs::InvalidEntityId.index),
            j.value("generation", rts::core::ecs::InvalidEntityId.generation)
        };
    }

    rts::core::ecs::EntityId remapEntityId(
        const rts::core::ecs::EntityId oldId,
        const std::unordered_map<std::uint64_t, rts::core::ecs::EntityId>& remap) {
        if (!rts::core::ecs::isValid(oldId)) {
            return rts::core::ecs::InvalidEntityId;
        }
        if (const auto it = remap.find(entityKey(oldId)); it != remap.end()) {
            return it->second;
        }
        return rts::core::ecs::InvalidEntityId;
    }

    nlohmann::json vecJson(const rts::core::model::Vector2D& v) {
        return nlohmann::json { { "x", v.x }, { "y", v.y } };
    }

    rts::core::model::Vector2D vecFromJson(const nlohmann::json& j) {
        return { j.value("x", 0.0f), j.value("y", 0.0f) };
    }

    nlohmann::json orderJson(const rts::core::model::UnitOrder& order) {
        return nlohmann::json {
            { "type", static_cast<int>(order.type) },
            { "targetEntityId", entityRef(order.targetEntityId) },
            { "targetPosition", vecJson(order.targetPosition) },
            { "abilityId", order.abilityId },
            { "buildingTypeId", order.buildingTypeId }
        };
    }

    rts::core::model::UnitOrder orderFromJson(
        const nlohmann::json& j,
        const std::unordered_map<std::uint64_t, rts::core::ecs::EntityId>& remap) {
        rts::core::model::UnitOrder order {};
        order.type = static_cast<rts::core::model::OrderType>(j.value("type", 0));
        order.targetEntityId = remapEntityId(entityIdFromJson(j.value("targetEntityId", nlohmann::json::object())), remap);
        order.targetPosition = vecFromJson(j.value("targetPosition", nlohmann::json::object()));
        order.abilityId = j.value("abilityId", -1);
        order.buildingTypeId = j.value("buildingTypeId", -1);
        return order;
    }

    nlohmann::json unitRuntimeJson(const rts::core::model::Unit::RuntimeState& state) {
        nlohmann::json queue = nlohmann::json::array();
        for (const auto& order : state.orderQueue) {
            queue.push_back(orderJson(order));
        }

        return nlohmann::json {
            { "action", static_cast<int>(state.action) },
            { "animationAction", static_cast<int>(state.animationAction) },
            { "moveTarget", vecJson(state.moveTarget) },
            { "finalTargetWorld", vecJson(state.finalTargetWorld) },
            { "attackMoveTarget", vecJson(state.attackMoveTarget) },
            { "patrolStart", vecJson(state.patrolStart) },
            { "patrolEnd", vecJson(state.patrolEnd) },
            { "patrolDestination", vecJson(state.patrolDestination) },
            { "attackTargetId", entityRef(state.attackTargetId) },
            { "buildTargetId", entityRef(state.buildTargetId) },
            { "attackMoveActive", state.attackMoveActive },
            { "patrolActive", state.patrolActive },
            { "patrolHeadingToEnd", state.patrolHeadingToEnd },
            { "attackRetargetRequested", state.attackRetargetRequested },
            { "orderQueue", queue },
            { "carryingType", static_cast<int>(state.carryingType) },
            { "carryingAmount", state.carryingAmount },
            { "maxCarryAmount", state.maxCarryAmount },
            { "gatherProgressSeconds", state.gatherProgressSeconds },
            { "gatherPhase", static_cast<int>(state.gatherPhase) },
            { "deliveryReady", state.deliveryReady },
            { "targetResourceId", entityRef(state.targetResourceId) },
            { "targetDropOffId", entityRef(state.targetDropOffId) },
            { "attackPhase", static_cast<int>(state.attackPhase) },
            { "attackTimer", state.attackTimer }
        };
    }

    rts::core::model::Unit::RuntimeState unitRuntimeFromJson(
        const nlohmann::json& j,
        const std::unordered_map<std::uint64_t, rts::core::ecs::EntityId>& remap) {
        rts::core::model::Unit::RuntimeState state {};
        state.action = static_cast<rts::core::model::ActionType>(j.value("action", 0));
        state.animationAction = static_cast<rts::core::model::ActionType>(j.value("animationAction", 0));
        state.moveTarget = vecFromJson(j.value("moveTarget", nlohmann::json::object()));
        state.finalTargetWorld = vecFromJson(j.value("finalTargetWorld", nlohmann::json::object()));
        state.attackMoveTarget = vecFromJson(j.value("attackMoveTarget", nlohmann::json::object()));
        state.patrolStart = vecFromJson(j.value("patrolStart", nlohmann::json::object()));
        state.patrolEnd = vecFromJson(j.value("patrolEnd", nlohmann::json::object()));
        state.patrolDestination = vecFromJson(j.value("patrolDestination", nlohmann::json::object()));
        state.attackTargetId = remapEntityId(entityIdFromJson(j.value("attackTargetId", nlohmann::json::object())), remap);
        state.buildTargetId = remapEntityId(entityIdFromJson(j.value("buildTargetId", nlohmann::json::object())), remap);
        state.attackMoveActive = j.value("attackMoveActive", false);
        state.patrolActive = j.value("patrolActive", false);
        state.patrolHeadingToEnd = j.value("patrolHeadingToEnd", true);
        state.attackRetargetRequested = j.value("attackRetargetRequested", false);
        for (const auto& order : j.value("orderQueue", nlohmann::json::array())) {
            state.orderQueue.push_back(orderFromJson(order, remap));
        }
        state.carryingType = static_cast<rts::core::model::ResourceNode::ResourceType>(j.value("carryingType", 0));
        state.carryingAmount = j.value("carryingAmount", 0);
        state.maxCarryAmount = j.value("maxCarryAmount", 10);
        state.gatherProgressSeconds = j.value("gatherProgressSeconds", 0.0f);
        state.gatherPhase = static_cast<rts::core::model::Unit::GatherPhase>(j.value("gatherPhase", 0));
        state.deliveryReady = j.value("deliveryReady", false);
        state.targetResourceId = remapEntityId(entityIdFromJson(j.value("targetResourceId", nlohmann::json::object())), remap);
        state.targetDropOffId = remapEntityId(entityIdFromJson(j.value("targetDropOffId", nlohmann::json::object())), remap);
        state.attackPhase = static_cast<rts::core::model::Unit::AttackPhase>(j.value("attackPhase", 0));
        state.attackTimer = j.value("attackTimer", 0.0f);
        return state;
    }

    nlohmann::json buildingRuntimeJson(const rts::core::model::Building::RuntimeState& state) {
        nlohmann::json queue = nlohmann::json::array();
        for (const auto type : state.trainQueue) {
            queue.push_back(static_cast<int>(type));
        }
        return nlohmann::json {
            { "queue", queue },
            { "trainTimer", state.trainTimer },
            { "rallyPoint", vecJson(state.rallyPoint) },
            { "hasRallyPoint", state.hasRallyPoint },
            { "completed", state.completed },
            { "buildTime", state.buildTime },
            { "buildProgress", state.buildProgress }
        };
    }

    rts::core::model::Building::RuntimeState buildingRuntimeFromJson(const nlohmann::json& j) {
        rts::core::model::Building::RuntimeState state {};
        for (const auto& type : j.value("queue", nlohmann::json::array())) {
            state.trainQueue.push_back(static_cast<rts::UnitType>(type.get<int>()));
        }
        state.trainTimer = j.value("trainTimer", 0.0f);
        state.rallyPoint = vecFromJson(j.value("rallyPoint", nlohmann::json::object()));
        state.hasRallyPoint = j.value("hasRallyPoint", false);
        state.completed = j.value("completed", true);
        state.buildTime = j.value("buildTime", 0.0f);
        state.buildProgress = j.value("buildProgress", 0.0f);
        return state;
    }

    void applyMapTerrain(
        rts::core::world::GameWorld& world,
        const rts::core::map::MapData& map) {
        world.initTileMap(map.width, map.height, map.tileSize);
        for (const auto& tile : map.blockedTiles) {
            world.setTileBlocked(tile.x, tile.y, true);
        }
    }

    const char* resultName(const rts::core::world::GameResult result) {
        using rts::core::world::GameResult;
        switch (result) {
            case GameResult::Victory: return "victory";
            case GameResult::Defeat: return "defeat";
            case GameResult::InProgress: return "in_progress";
        }
        return "unknown";
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
            setupInitialWorld(defaultMapPath());
        }

        // If the lobby asked us to play back a specific replay, load it now;
        // otherwise record this live match so it can be reviewed afterwards.
        if (auto replay = core::app::SessionContext::instance().takeReplayToPlay()) {
            startReplayFrom(*replay);
        } else {
            beginRecording();
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
                beginRecording();  // a fresh match records from the start again
            }
        });

        // Select a single entity by handle (multi-selection portrait click).
        m_router.on<command::SelectEntityCommand>([this](const command::SelectEntityCommand &cmd) {
            auto lock = m_world.acquireWriteLock();
            const core::ecs::EntityId id{ cmd.index(), cmd.generation() };
            if (auto element = m_world.resolve(id)) {
                SelectionSystem::SelectedList one;
                one.push_back(element);
                m_selection.replaceSelected(std::move(one));
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

        // Leave the match (ESC): persist the recorded replay, then return to the lobby.
        m_router.on<command::QuitToLobbyCommand>([this](const command::QuitToLobbyCommand &) {
            saveReplayToAppData();
            m_bus.push(std::make_unique<command::SceneChangeCommand>("lobby"));
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
        // A finished replay freezes on its final frame: no further simulation, and
        // input stays locked (replayActive) until the viewer leaves to the lobby.
        if (m_simFrozen) {
            return;
        }
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
        handleIdleAutoAcquire();
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
                if (const auto h = m_replay.hashForTick(t)) {
                    const auto actual = m_world.worldHash();
                    if (*h != actual) {
                        std::cerr << "[Replay] divergence at tick " << t
                                  << ": expected " << *h
                                  << ", actual " << actual
                                  << " (check economy, entity HP/action/position, "
                                     "unit orders/targets, production/build progress, "
                                     "rally points, or projectiles)\n";
                    }
                }
                if (t >= m_replay.lastTick() && !m_simFrozen) {
                    // Freeze on the final frame and keep input locked (mode stays Play,
                    // replayActive stays true) so the finished replay never becomes a
                    // live, controllable match. The viewer leaves with ESC.
                    m_simFrozen = true;
                    std::cout << "[Replay] playback finished at tick " << t << " (frozen)\n";
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
        // Range is gated per-candidate (weapon range + that target's body radius),
        // so the query is widened by the largest footprint reach to still surface a
        // big building whose center sits just past weapon range.
        const float queryRadius = unit.getAttackRange() + kMaxTargetHitRadius;

        for (auto candidate : world::queryRadius(m_world, unit.getPosition(), queryRadius)) {
            if (!candidate || !unit.canAttackTarget(candidate.get())) {
                continue;
            }

            const float effectiveRange = unit.getAttackRange() + candidate->attackHitRadius();
            const float candidateDistanceSq = distanceSq(
                candidate->getPosition(),
                unit.getPosition()
            );
            if (candidateDistanceSq <= effectiveRange * effectiveRange &&
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

    std::shared_ptr<model::IGameElement> GameLogicManager::findClosestIdleTarget(
        const model::Unit& unit) const {
        std::shared_ptr<model::IGameElement> bestTarget;
        float bestDistanceSq = std::numeric_limits<float>::max();
        // An idle unit watches its full sight radius for enemies to engage, with
        // the shared acquire radius and its own weapon range as lower bounds so a
        // long-range or wide-sighted unit still reaches what it can perceive.
        const float sightWorld = unit.getSightRange() * m_world.gridTransform().tileSize;
        const float acquireRadius = std::max(
            { kAttackMoveAcquireRadius, unit.getAttackRange(), sightWorld });
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

    void GameLogicManager::handleIdleAutoAcquire() {
        for (const auto& element : m_world.getElements()) {
            auto unit = std::dynamic_pointer_cast<model::Unit>(element);
            // Only genuinely idle, combat-capable units acquire on their own.
            // Move/Attack/Hold/Gather/Build all report a non-Idle action, so a unit
            // carrying out any explicit order is skipped here. Workers keep mining,
            // and units already steered by another pass (attack-move, patrol,
            // post-kill retarget, or a queued order) are left to that pass.
            if (!unit || unit->getAction() != model::ActionType::Idle) {
                continue;
            }
            if (unit->isWorker() ||
                unit->isAttackMoveActive() ||
                unit->isPatrolActive() ||
                unit->needsAttackRetarget() ||
                unit->hasQueuedOrders()) {
                continue;
            }

            if (auto target = findClosestIdleTarget(*unit)) {
                unit->attack(target.get());
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
    void GameLogicManager::setupInitialWorld(const std::string& mapPath) {
        // Scenario maps are runtime data. Keeping the selected path here lets replay
        // playback rebuild the same initial world instead of always using default.
        m_currentMapPath = mapPath.empty() ? defaultMapPath() : mapPath;
        const auto map = core::map::loadMap(m_currentMapPath);

        applyMapTerrain(m_world, map);

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
        m_simFrozen = false;
        world::resetRuntimeServices(m_world);
        setupInitialWorld(m_currentMapPath.empty() ? defaultMapPath() : m_currentMapPath);
        world::rebuildSpatialIndex(m_world);
    }

    std::string GameLogicManager::quickSavePath() {
        return std::string(core::data::DataRoot) + "/saves/quicksave.json";
    }

    std::string GameLogicManager::defaultMapPath() {
        // Portfolio showcase scenario (data/maps/portfolio.tmx; portfolio.json is
        // the same layout). Revert to "/maps/tiled_skirmish.tmx" for the original.
        return std::string(core::data::DataRoot) + "/maps/portfolio.tmx";
    }

    bool GameLogicManager::saveGame(const std::string& path) {
        using json = nlohmann::json;
        json doc;
        {
            auto lock = m_world.acquireReadLock();
            doc["version"] = 2;
            doc["map"] = m_currentMapPath.empty() ? defaultMapPath() : m_currentMapPath;
            doc["tick"] = static_cast<std::uint64_t>(m_world.currentTick());
            doc["worldHash"] = m_world.worldHash();
            doc["gameResult"] = static_cast<int>(m_world.gameResult());
            doc["ai"] = {
                { "state", static_cast<int>(m_aiState) },
                { "produceTimer", m_aiProduceTimer },
                { "gatherTimer", m_aiGatherTimer },
                { "waveTimer", m_aiWaveTimer },
                { "defenseTimer", m_aiDefenseTimer }
            };

            json players = json::array();
            for (const int team : { core::model::TeamId::Player, core::model::TeamId::Enemy }) {
                const auto& r = m_world.playerResources(team);
                players.push_back({
                    { "team", team }, { "gold", r.gold }, { "wood", r.wood },
                    { "foodUsed", r.foodUsed }, { "foodCapacity", r.foodCapacity }, { "army", r.army }
                });
            }
            doc["players"] = players;

            json entities = json::array();
            for (const auto& el : m_world.getElements()) {
                auto ge = std::dynamic_pointer_cast<core::model::IGameElement>(el);
                if (!ge || ge->getAction() == core::model::ActionType::Dead) {
                    continue;
                }
                const auto pos = ge->getPosition();
                json entity {
                    { "saveId", entityRef(ge->entityId()) },
                    { "position", vecJson(pos) },
                    { "team", ge->getTeamId() }
                };
                if (auto u = std::dynamic_pointer_cast<core::model::Unit>(el)) {
                    entity["kind"] = "unit";
                    entity["type"] = static_cast<int>(u->unitType());
                    entity["hp"] = u->getHp();
                    entity["runtime"] = unitRuntimeJson(u->runtimeState());
                } else if (auto b = std::dynamic_pointer_cast<core::model::Building>(el)) {
                    entity["kind"] = "building";
                    entity["type"] = static_cast<int>(b->buildingType());
                    entity["hp"] = b->getHp();
                    entity["runtime"] = buildingRuntimeJson(b->runtimeState());
                } else if (auto rn = std::dynamic_pointer_cast<core::model::ResourceNode>(el)) {
                    entity["kind"] = "resource";
                    entity["type"] = static_cast<int>(rn->type());
                    entity["totalAmount"] = rn->totalAmount();
                    entity["remaining"] = rn->remaining();
                    entity["gatherAmount"] = rn->gatherAmountPerTrip();
                    entity["gatherDurationSeconds"] = rn->gatherDurationSeconds();
                    entity["maxGatherers"] = rn->maxGatherers();
                } else {
                    continue;
                }
                entities.push_back(std::move(entity));
            }
            doc["entities"] = entities;
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
        m_simFrozen = false;
        world::resetRuntimeServices(m_world);

        m_currentMapPath = doc.value("map", m_currentMapPath.empty() ? defaultMapPath() : m_currentMapPath);
        const auto map = core::map::loadMap(m_currentMapPath);
        applyMapTerrain(m_world, map);

        for (const auto& p : doc.value("players", json::array())) {
            core::model::PlayerResourceState r {};
            r.gold = p.value("gold", r.gold);
            r.wood = p.value("wood", r.wood);
            r.foodUsed = p.value("foodUsed", 0);
            r.foodCapacity = p.value("foodCapacity", 0);
            r.army = p.value("army", 0);
            m_world.setPlayerResources(p.value("team", core::model::TeamId::Neutral), r);
        }

        std::unordered_map<std::uint64_t, core::ecs::EntityId> entityRemap;
        std::vector<std::pair<std::shared_ptr<core::model::Unit>, json>> pendingUnitRuntime;
        std::vector<std::pair<std::shared_ptr<core::model::Building>, json>> pendingBuildingRuntime;

        if (doc.contains("entities")) {
            for (const auto& e : doc.value("entities", json::array())) {
                const std::string kind = e.value("kind", std::string {});
                const auto oldId = entityIdFromJson(e.value("saveId", json::object()));
                const auto pos = vecFromJson(e.value("position", json::object()));
                const int team = e.value("team", core::model::TeamId::Neutral);

                std::shared_ptr<core::model::IGameElement> created;
                if (kind == "building") {
                    const auto type = static_cast<core::model::BuildingType>(e.value("type", 0));
                    auto building = std::make_shared<core::model::Building>(type, pos, team);
                    registerBuildingSpawn(*building);
                    building->setHp(e.value("hp", building->getMaxHp()));
                    created = building;
                    pendingBuildingRuntime.emplace_back(building, e.value("runtime", json::object()));
                } else if (kind == "unit") {
                    auto unit = std::make_shared<core::model::Unit>(
                        static_cast<::rts::UnitType>(e.value("type", 0)));
                    unit->setPosition(pos);
                    unit->setTeamId(team);
                    unit->setHp(e.value("hp", unit->getMaxHp()));
                    created = unit;
                    pendingUnitRuntime.emplace_back(unit, e.value("runtime", json::object()));
                } else if (kind == "resource") {
                    const auto type = static_cast<core::model::ResourceNode::ResourceType>(e.value("type", 0));
                    const auto data = core::data::resourceStaticDataFor(type);
                    auto node = std::make_shared<core::model::ResourceNode>(
                        pos,
                        type,
                        e.value("totalAmount", data.initialAmount),
                        e.value("gatherAmount", data.gatherAmountPerTrip),
                        e.value("gatherDurationSeconds", data.gatherDurationSeconds),
                        e.value("maxGatherers", data.maxGatherers));
                    node->setRemaining(e.value("remaining", data.initialAmount));
                    created = node;
                }

                if (created) {
                    m_world.addElement(created);
                    if (core::ecs::isValid(oldId)) {
                        entityRemap[entityKey(oldId)] = created->entityId();
                    }
                }
            }

            for (const auto& [building, runtime] : pendingBuildingRuntime) {
                building->restoreRuntimeState(buildingRuntimeFromJson(runtime));
            }
            for (const auto& [unit, runtime] : pendingUnitRuntime) {
                unit->restoreRuntimeState(unitRuntimeFromJson(runtime, entityRemap));
            }
        } else {
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
        }

        if (const auto ai = doc.value("ai", json::object()); !ai.empty()) {
            m_aiState = static_cast<AiBuildOrderState>(ai.value("state", 0));
            m_aiProduceTimer = ai.value("produceTimer", 0.0f);
            m_aiGatherTimer = ai.value("gatherTimer", 0.0f);
            m_aiWaveTimer = ai.value("waveTimer", 0.0f);
            m_aiDefenseTimer = ai.value("defenseTimer", 0.0f);
            m_lastFeedbackAiState = m_aiState;
        }

        m_world.setGameResult(static_cast<core::world::GameResult>(
            doc.value("gameResult", static_cast<int>(core::world::GameResult::InProgress))));
        m_world.setCurrentTick(static_cast<core::sim::TickCount>(doc.value("tick", 0ull)));
        world::rebuildSpatialIndex(m_world);
        recomputeSupply();
        m_world.updateFog();

        if (doc.contains("worldHash")) {
            const auto expected = doc.value("worldHash", 0ull);
            const auto actual = m_world.worldHash();
            if (expected == actual) {
                std::cout << "[LoadGame] world hash matched " << actual << "\n";
            } else {
                std::cerr << "[LoadGame] world hash changed after load: saved "
                          << expected << ", loaded " << actual
                          << " (check EntityId gaps, transient pathing, or unsaved sim fields)\n";
            }
        }

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
            m_replay.setMetadata(resultName(m_world.gameResult()),
                                 static_cast<std::uint64_t>(m_world.currentTick()));
            m_replay.save(replayPath());
            m_replayMode = ReplayMode::Off;
            std::cout << "[Replay] recording stopped and saved\n";
            return;
        }
        // Begin from a known initial state so the stream replays deterministically.
        restartMatch();
        beginRecording();
        std::cout << "[Replay] recording started\n";
    }

    void GameLogicManager::beginRecording() {
        // Caller has already set up the starting world; capture the stream from tick 0.
        m_replay.clear();
        m_replay.setMapPath(m_currentMapPath.empty() ? defaultMapPath() : m_currentMapPath);
        m_replay.setMetadata("in_progress", 0);
        m_replayMode = ReplayMode::Record;
        m_replaySaved = false;
        m_simFrozen = false;
        m_world.setReplayActive(false);  // live match: player input applies
    }

    void GameLogicManager::startReplay() {
        core::replay::ReplayLog loaded;
        if (!loaded.load(replayPath())) {
            return;
        }
        m_replay = std::move(loaded);
        m_currentMapPath = m_replay.mapPath().empty() ? defaultMapPath() : m_replay.mapPath();
        restartMatch();  // rewind to the initial state the replay was recorded from
        m_replayMode = ReplayMode::Play;
        m_replaySaved = true;  // a replayed match is never re-saved
        m_simFrozen = false;
        m_world.setReplayActive(true);  // viewer-only: ignore live player input
        std::cout << "[Replay] playback started on " << m_currentMapPath
                  << " (" << m_replay.lastTick() << " ticks)\n";
    }

    void GameLogicManager::startReplayFrom(const std::string& path) {
        core::replay::ReplayLog loaded;
        if (!loaded.load(path)) {
            std::cout << "[Replay] failed to load " << path << " (recording instead)\n";
            beginRecording();
            return;
        }
        m_replay = std::move(loaded);
        m_currentMapPath = m_replay.mapPath().empty() ? defaultMapPath() : m_replay.mapPath();
        restartMatch();  // replay files own their map path
        m_replayMode = ReplayMode::Play;
        m_replaySaved = true;  // a replayed match is never re-saved
        m_simFrozen = false;
        m_world.setReplayActive(true);  // viewer-only: ignore live player input
        std::cout << "[Replay] playback started from " << path
                  << " on " << m_currentMapPath
                  << " (" << m_replay.lastTick() << " ticks, result "
                  << m_replay.result() << ")\n";
    }

    void GameLogicManager::saveReplayToAppData() {
        if (m_replayMode != ReplayMode::Record || m_replaySaved) {
            return;
        }
        m_replaySaved = true;
        m_replay.setMetadata(resultName(m_world.gameResult()),
                             static_cast<std::uint64_t>(m_world.currentTick()));
        std::time_t now = std::time(nullptr);
        char stamp[32] {};
        std::strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", std::localtime(&now));
        const std::string path =
            core::app::SessionContext::replaysDir() + "/replay_" + stamp + ".json";
        if (m_replay.save(path)) {
            std::cout << "[Replay] saved to " << path << "\n";
        }
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
            saveReplayToAppData();
        } else if (enemyHalls == 0) {
            m_world.setGameResult(core::world::GameResult::Victory);
            world::emitSound(m_world, world::SoundCue::Victory, {}, 80.0f);
            saveReplayToAppData();
        }
    }

    void GameLogicManager::captureFeedbackSnapshots() {
        std::unordered_map<std::uint64_t, ElementSnapshot> next;
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

            const auto eid = gameElement->entityId();
            const auto id = (static_cast<std::uint64_t>(eid.index) << 32) | eid.generation;
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
                    // Brief death puff only; no lingering blood/scorch decal at the spot.
                    world::emitEffect(m_world, world::EffectType::DeathBurst, pos, 52.0f, 0.6f);
                }
                if (!prev.complete && complete) {
                    world::emitSound(m_world, world::SoundCue::ConstructionComplete,
                                     gameElement->getPosition(), 62.0f);
                    world::emitEffect(m_world, world::EffectType::ConstructionDust,
                                      gameElement->getPosition(), 82.0f, 0.75f);
                }
            }
            // Dead elements linger in the world; don't keep tracking them (the
            // death transition already fired) so they can't seed phantom feedback.
            if (action != model::ActionType::Dead) {
                next[id] = ElementSnapshot {
                    .hp = hp,
                    .complete = complete,
                    .action = action
                };
            }
        }
        m_feedbackSnapshots.swap(next);
    }

    bool GameLogicManager::inputLocked() const {
        return m_world.gameResult() != core::world::GameResult::InProgress;
    }
}
