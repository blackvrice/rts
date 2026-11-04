#include "game/game/GameLogicManager.hpp"

#include <algorithm>
#include <ctime>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <utility>

#include <nlohmann/json.hpp>

#include "core/app/SessionContext.hpp"
#include "core/command/LogicCommand.hpp"
#include "core/data/DataPaths.hpp"
#include "core/data/ResourceStaticData.hpp"
#include "core/map/MapData.hpp"
#include "core/model/Building.hpp"
#include "core/model/PlayerResourceState.hpp"
#include "core/model/ResourceNode.hpp"
#include "core/model/Unit.hpp"
#include "core/world/GameWorld.hpp"
#include "core/world/WorldRuntimeServices.hpp"
#include "game/game/systems/ConstructionSystem.hpp"
#include "game/game/systems/GameplayQueries.hpp"
#include "game/game/systems/MatchRules.hpp"
#include "game/game/systems/MatchSnapshot.hpp"

namespace {
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
        core::world::GameWorld &world,
        core::app::MatchConfig config)
        : ILogicManager(bus, router), m_world(world), m_config(std::move(config)) {
        // Populate the initial match state. Extracted into setupInitialWorld so a
        // restart can rebuild the same starting position.
        {
            auto lock = m_world.acquireWriteLock();
            setupInitialWorld(m_config.mapPath.empty() ? defaultMapPath() : m_config.mapPath);
        }

        // If the lobby asked us to play back a specific replay, load it now;
        // otherwise record this live match so it can be reviewed afterwards.
        if (auto replay = core::app::SessionContext::instance().takeReplayToPlay()) {
            startReplayFrom(*replay);
        } else if (m_config.recordReplay) {
            beginRecording();
        }

        registerCommandHandlers();
    }

    void GameLogicManager::registerCommandHandlers() {
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
                if (m_config.recordReplay) {
                    beginRecording();  // a fresh match records from the start again
                }
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

        // Unit orders: gate (replay/record), lock, reject once the match is over,
        // then hand off to UnitOrderSystem.
        m_router.on<command::MoveCommand>([this](const command::MoveCommand &cmd) {
            if (!acceptPlayerCommand(cmd)) return;
            auto lock = m_world.acquireWriteLock();
            if (inputLocked()) return;
            m_orders.move(m_world, m_selection.selected(), cmd);
        });

        m_router.on<command::AttackCommand>([this](const command::AttackCommand &cmd) {
            if (!acceptPlayerCommand(cmd)) return;
            auto lock = m_world.acquireWriteLock();
            if (inputLocked()) return;
            m_orders.attack(m_world, m_selection.selected(), cmd);
        });

        m_router.on<command::AttackMoveCommand>([this](const command::AttackMoveCommand &cmd) {
            if (!acceptPlayerCommand(cmd)) return;
            auto lock = m_world.acquireWriteLock();
            if (inputLocked()) return;
            m_orders.attackMove(m_world, m_selection.selected(), cmd);
        });

        m_router.on<command::GatherCommand>([this](const command::GatherCommand &cmd) {
            if (!acceptPlayerCommand(cmd)) return;
            auto lock = m_world.acquireWriteLock();
            if (inputLocked()) return;
            m_orders.gather(m_world, m_selection.selected(), cmd);
        });

        m_router.on<command::PatrolCommand>([this](const command::PatrolCommand &cmd) {
            if (!acceptPlayerCommand(cmd)) return;
            auto lock = m_world.acquireWriteLock();
            if (inputLocked()) return;
            m_orders.patrol(m_world, m_selection.selected(), cmd);
        });

        m_router.on<command::StopCommand>([this](const command::StopCommand &cmd) {
            if (!acceptPlayerCommand(cmd)) return;
            auto lock = m_world.acquireWriteLock();
            if (inputLocked()) return;
            m_orders.stop(m_selection.selected());
        });

        m_router.on<command::HoldPositionCommand>([this](const command::HoldPositionCommand &cmd) {
            if (!acceptPlayerCommand(cmd)) return;
            auto lock = m_world.acquireWriteLock();
            if (inputLocked()) return;
            m_orders.hold(m_selection.selected());
        });

        // Production and construction act on the first selected building/worker.
        m_router.on<command::TrainUnitCommand>([this](const command::TrainUnitCommand &cmd) {
            if (!acceptPlayerCommand(cmd)) return;
            auto lock = m_world.acquireWriteLock();
            if (inputLocked()) return;
            if (auto building = queries::firstSelectedBuilding(m_selection.selected())) {
                m_production.train(m_world, *building, cmd.unitTypeId());
            }
        });

        m_router.on<command::CancelProductionCommand>([this](const command::CancelProductionCommand &cmd) {
            if (!acceptPlayerCommand(cmd)) return;
            auto lock = m_world.acquireWriteLock();
            if (inputLocked()) return;
            if (auto building = queries::firstSelectedBuilding(m_selection.selected())) {
                m_production.cancelLast(m_world, *building);
            }
        });

        m_router.on<command::BuildCommand>([this](const command::BuildCommand &cmd) {
            if (!acceptPlayerCommand(cmd)) return;
            auto lock = m_world.acquireWriteLock();
            if (inputLocked()) return;
            if (auto worker = queries::firstSelectedWorker(m_selection.selected())) {
                ConstructionSystem::orderWorkerToBuild(
                    m_world, m_production, m_movement, *worker,
                    cmd.buildingTypeId(), cmd.position());
            }
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

        // Fixed system order. Changing it changes simulation results (and replay
        // hashes), so treat any reordering as a gameplay change.
        m_world.advanceTick();
        world::updateRuntimeServices(m_world, dt);
        m_movement.update(m_world, dt, m_collision);
        world::rebuildSpatialIndex(m_world);
        m_world.updateProjectiles(dt);
        m_combat.update(m_world, m_movement, m_config.enemyAiPassive);
        m_orders.handleQueuedOrders(m_world);
        m_economy.applyReadyResourceDeliveries(m_world);
        m_economy.handleGatherRedirects(m_world);
        m_production.flushPendingSpawns(m_world, m_movement);
        m_economy.recomputeSupply(m_world);
        m_world.updateFog();
        m_feedback.capture(m_world);
        // Destroy EntityIds of units/buildings that died this tick so handles to
        // them stop validating (and recycled slots bump generation).
        m_world.pruneDeadEntities();
        for (auto& ai : m_ais) {
            ai.update(m_world, m_movement, m_production, dt, m_config.enemyAiPassive);
        }
        if (MatchRules::checkVictoryDefeat(m_world)) {
            saveReplayToAppData();
        }

        updateReplayCheckpoint();
    }

    bool GameLogicManager::inputLocked() const {
        return m_world.gameResult() != core::world::GameResult::InProgress;
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
        m_world.setPlayers(buildPlayerSlots(map), m_config.localPlayerId);
        createAiControllers();

        const auto makeResources = [](int gold, int wood) {
            core::model::PlayerResourceState r {};
            r.gold = gold;
            r.wood = wood;
            r.foodUsed = 0;
            r.foodCapacity = 20;
            r.army = 0;
            return r;
        };
        for (const auto& slot : m_world.players()) {
            const auto start = map.startResourcesFor(slot.id);
            m_world.setPlayerResources(slot.id, makeResources(start.gold, start.wood));
        }

        for (const auto& b : map.buildings) {
            auto building = std::make_shared<core::model::Building>(b.type, b.position, b.teamId);
            m_production.registerBuildingSpawn(*building);
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

    void GameLogicManager::resetMatchState() {
        m_world.resetForNewMatch();
        m_selection.clear();
        m_movement.reset();
        m_production.clear();
        m_ais.clear();
        m_feedback.clear();
        m_simFrozen = false;
        world::resetRuntimeServices(m_world);
    }

    std::vector<core::model::PlayerSlot> GameLogicManager::buildPlayerSlots(
        const core::map::MapData& map) const {
        std::vector<core::model::PlayerSlot> slots = m_config.players;
        for (const int id : map.playerIds()) {
            const bool configured = std::any_of(slots.begin(), slots.end(),
                [id](const auto& slot) { return slot.id == id; });
            if (configured) continue;
            slots.push_back(core::model::PlayerSlot {
                id,
                id,  // own alliance: free-for-all by default
                id == m_config.localPlayerId ? core::model::PlayerController::Human
                                             : core::model::PlayerController::Ai,
                core::model::defaultPlayerColor(id),
                false
            });
        }
        return slots;
    }

    void GameLogicManager::createAiControllers() {
        m_ais.clear();
        for (const auto& slot : m_world.players()) {
            if (slot.controller == core::model::PlayerController::Ai) {
                m_ais.emplace_back(slot.id);
            }
        }
    }

    void GameLogicManager::restartMatch() {
        auto lock = m_world.acquireWriteLock();
        resetMatchState();
        setupInitialWorld(m_currentMapPath.empty() ? defaultMapPath() : m_currentMapPath);
        world::rebuildSpatialIndex(m_world);
    }

    std::string GameLogicManager::saveDirectory() const {
        return m_config.saveDirectory.empty()
            ? std::string(core::data::DataRoot) + "/saves"
            : m_config.saveDirectory;
    }

    std::string GameLogicManager::quickSavePath() const {
        return saveDirectory() + "/quicksave.json";
    }

    std::string GameLogicManager::defaultMapPath() {
        // Portfolio showcase scenario (data/maps/portfolio.tmx; portfolio.json is
        // the same layout). Revert to "/maps/tiled_skirmish.tmx" for the original.
        return std::string(core::data::DataRoot) + "/maps/portfolio.tmx";
    }

    // =========================================================
    // Save / Load (format lives in systems/MatchSnapshot)
    // =========================================================
    bool GameLogicManager::saveGame(const std::string& path) {
        nlohmann::json doc;
        {
            auto lock = m_world.acquireReadLock();
            nlohmann::json aiStates = nlohmann::json::array();
            for (const auto& ai : m_ais) {
                auto state = ai.toJson();
                state["player"] = ai.playerId();
                aiStates.push_back(std::move(state));
            }
            doc = snapshot::capture(
                m_world,
                m_currentMapPath.empty() ? defaultMapPath() : m_currentMapPath,
                m_config.enemyAiPassive,
                aiStates);
            // System state that lives outside the world: queued path requests and
            // trained units still waiting for room to spawn.
            doc["movement"] = m_movement.saveState(m_world);
            doc["production"] = m_production.saveState();
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
        resetMatchState();

        m_currentMapPath = doc.value("map", m_currentMapPath.empty() ? defaultMapPath() : m_currentMapPath);
        // Saves without the field keep the running match's AI mode.
        m_config.enemyAiPassive = doc.value("enemyAiPassive", m_config.enemyAiPassive);
        const auto map = core::map::loadMap(m_currentMapPath);
        applyMapTerrain(m_world, map);

        snapshot::restore(m_world, doc, [this](core::model::Building& building) {
            m_production.registerBuildingSpawn(building);
        });

        // Slots come from the snapshot, so the AI set is rebuilt from them.
        createAiControllers();
        const auto restoreAi = [this](const int playerId, const json& state) {
            for (auto& ai : m_ais) {
                if (ai.playerId() == playerId) {
                    ai.fromJson(state);
                }
            }
        };
        if (doc.contains("ais")) {
            for (const auto& state : doc.value("ais", json::array())) {
                restoreAi(state.value("player", core::model::TeamId::Neutral), state);
            }
        } else if (const auto ai = doc.value("ai", json::object()); !ai.empty()) {
            restoreAi(core::model::TeamId::Enemy, ai);  // v2: the single enemy AI
        }
        if (doc.contains("movement")) {
            m_movement.restoreState(m_world, doc["movement"]);
        }
        if (doc.contains("production")) {
            m_production.restoreState(doc["production"]);
        }

        m_world.setGameResult(static_cast<core::world::GameResult>(
            doc.value("gameResult", static_cast<int>(core::world::GameResult::InProgress))));
        m_world.setCurrentTick(static_cast<core::sim::TickCount>(doc.value<std::uint64_t>("tick", 0)));
        world::rebuildSpatialIndex(m_world);
        // v4 saves are exact, so the saved economy is authoritative (a tick-0 save
        // still has the map's initial food values; the next tick recounts them).
        // Older saves derive food/army from the restored entities.
        if (doc.value("version", 0) < 4) {
            m_economy.recomputeSupply(m_world);
        }
        m_world.updateFog();

        if (doc.contains("worldHash")) {
            const auto expected = doc.value<std::uint64_t>("worldHash", 0);
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
    std::string GameLogicManager::replayPath() const {
        return saveDirectory() + "/replay.json";
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
        m_replay.setEnemyAiPassive(m_config.enemyAiPassive);
        m_replay.setMetadata("in_progress", 0);
        m_replayMode = ReplayMode::Record;
        m_replaySaved = false;
        m_simFrozen = false;
        m_world.setReplayActive(false);  // live match: player input applies
    }

    void GameLogicManager::beginPlayback() {
        m_currentMapPath = m_replay.mapPath().empty() ? defaultMapPath() : m_replay.mapPath();
        // Playback must run the AI rules the replay was recorded under.
        m_config.enemyAiPassive = m_replay.enemyAiPassive();
        restartMatch();  // replay files own their map path; rewind to their start
        m_replayMode = ReplayMode::Play;
        m_replaySaved = true;  // a replayed match is never re-saved
        m_simFrozen = false;
        m_world.setReplayActive(true);  // viewer-only: ignore live player input
    }

    void GameLogicManager::startReplay() {
        core::replay::ReplayLog loaded;
        if (!loaded.load(replayPath())) {
            return;
        }
        m_replay = std::move(loaded);
        beginPlayback();
        std::cout << "[Replay] playback started on " << m_currentMapPath
                  << " (" << m_replay.lastTick() << " ticks)\n";
    }

    void GameLogicManager::startReplayFrom(const std::string& path) {
        core::replay::ReplayLog loaded;
        if (!loaded.load(path)) {
            std::cout << "[Replay] failed to load " << path << " (recording instead)\n";
            if (m_config.recordReplay) {
                beginRecording();
            }
            return;
        }
        m_replay = std::move(loaded);
        beginPlayback();
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

    void GameLogicManager::updateReplayCheckpoint() {
        // Replay: record a periodic world-hash checkpoint while recording; compare
        // it while playing (logging divergence), and stop when the stream is spent.
        if (m_replayMode == ReplayMode::Off) {
            return;
        }
        const auto t = static_cast<std::uint64_t>(m_world.currentTick());
        if (m_replayMode == ReplayMode::Record) {
            if (t % 30 == 0) {
                m_replay.checkpoint(t, m_world.worldHash());
            }
            return;
        }

        // Play
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
