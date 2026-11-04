//
// Created by black on 25. 12. 25..
//

#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/app/MatchConfig.hpp"
#include "core/manager/ILogicManager.hpp"
#include "core/replay/ReplayLog.hpp"
#include "game/game/systems/AiController.hpp"
#include "game/game/systems/CollisionSystem.hpp"
#include "game/game/systems/CombatSystem.hpp"
#include "game/game/systems/ControlGroupSystem.hpp"
#include "game/game/systems/EconomySystem.hpp"
#include "game/game/systems/FeedbackSystem.hpp"
#include "game/game/systems/MovementSystem.hpp"
#include "game/game/systems/ProductionSystem.hpp"
#include "game/game/systems/SelectionSystem.hpp"
#include "game/game/systems/UnitOrderSystem.hpp"

namespace rts::core::world {
    class GameWorld;
}

namespace rts::core::map {
    struct MapData;
}

namespace rts::core::manager {
    // Owns one match's simulation: routes LogicCommands to the gameplay systems,
    // runs the fixed per-tick system order, and handles match lifecycle
    // (setup/restart), save/load, and replay record/playback. Gameplay rules live
    // in the systems under game/game/systems; this class only coordinates them.
    class GameLogicManager final : public ILogicManager {
    public:
        GameLogicManager(command::LogicCommandBus &bus,
                         command::LogicCommandRouter &router,
                        core::world::GameWorld &world,
                        core::app::MatchConfig config = {}
                         );

        const core::app::MatchConfig& matchConfig() const noexcept { return m_config; }

        void update() override;

        void tick(float dt) override;

    private:
        void registerCommandHandlers();
        bool inputLocked() const;

        // Match lifecycle
        void setupInitialWorld(const std::string& mapPath);  // spawns map data (caller holds the lock)
        void restartMatch();       // resets the world and repopulates the starting position
        // Clears systems and per-match state before the world is rebuilt (restart/load).
        void resetMatchState();
        // Player slots for a new match: MatchConfig::players when given, plus every
        // player id the map uses (local = Human, others = AI, FFA alliances).
        std::vector<model::PlayerSlot> buildPlayerSlots(const map::MapData& map) const;
        // One AiController per AI-controlled slot, in slot-id order.
        void createAiControllers();
        // Save / load the live match to a JSON snapshot (tick, economy, every
        // unit/building/resource's major state). Returns false on I/O/parse failure.
        bool saveGame(const std::string& path);
        bool loadGame(const std::string& path);
        // Quick-save slot and F6/F7 replay slot inside MatchConfig::saveDirectory
        // (default <data>/saves).
        std::string saveDirectory() const;
        std::string quickSavePath() const;
        static std::string defaultMapPath();

        // Replay control. Recording taps every player command (stamped with the tick
        // it applies on); playback restarts the match and re-dispatches that stream.
        enum class ReplayMode { Off, Record, Play };
        std::string replayPath() const;
        void toggleRecording();   // start recording, or stop and write the replay file
        void startReplay();       // load the replay, reset to the start, and play it back
        void startReplayFrom(const std::string& path);  // play a specific replay file
        // Switches to playback of an already loaded replay (restarts on its map).
        void beginPlayback();
        // Begins recording the current (already set up) match from tick 0.
        void beginRecording();
        // Saves the recorded replay to AppData with a timestamped name (once per match).
        void saveReplayToAppData();
        // Gate + record a live player command: returns false when it should be
        // ignored (live input during playback), records it while in Record mode.
        bool acceptPlayerCommand(const command::LogicCommand& cmd);
        // Re-dispatches the recorded commands stamped for a tick (playback).
        void applyReplayCommands(std::uint64_t tick);
        // Records (Record) or verifies (Play) the periodic WorldHash checkpoint and
        // freezes playback on its final tick.
        void updateReplayCheckpoint();

        core::world::GameWorld& m_world;
        // Rules for this match (map, AI mode, replay recording). Replay playback
        // and save loading overwrite the AI mode with the recorded value.
        core::app::MatchConfig m_config;

        // Systems, in dependency order (UnitOrderSystem keeps references to
        // movement and economy, so those are declared first).
        SelectionSystem m_selection;
        ControlGroupSystem m_controlGroups;
        CollisionSystem m_collision;
        MovementSystem m_movement;
        EconomySystem m_economy;
        ProductionSystem m_production;
        CombatSystem m_combat;
        UnitOrderSystem m_orders { m_movement, m_economy };
        std::vector<AiController> m_ais;  // one per AI slot, slot-id order
        FeedbackSystem m_feedback;

        // Replay: command stream + mode. m_applyingReplay is set only while the tick
        // re-dispatches recorded commands, so the gate lets them through.
        core::replay::ReplayLog m_replay;
        std::string m_currentMapPath;
        ReplayMode m_replayMode { ReplayMode::Off };
        bool m_applyingReplay { false };
        bool m_replaySaved { false };  // guards one auto-save per recorded match
        // Set when a playback reaches the end of the recorded stream: the simulation
        // is frozen on its final frame (no advance/AI) and input stays locked so the
        // finished replay never silently turns into a live, controllable match.
        bool m_simFrozen { false };
    };
} // namespace rts::manager
