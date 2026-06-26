//
// Created by black on 25. 12. 25..
//

#pragma once
#include <cstdint>
#include <unordered_map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/manager/ILogicManager.hpp"
#include "core/model/PlayerResourceState.hpp"
#include "core/model/ResourceNode.hpp"
#include "core/model/UnitType.hpp"
#include "core/model/Vector2D.hpp"
#include "core/replay/ReplayLog.hpp"
#include "core/tech/TechTreeValidator.hpp"
#include "game/game/systems/CollisionSystem.hpp"
#include "game/game/systems/ControlGroupSystem.hpp"
#include "game/game/systems/MovementSystem.hpp"
#include "game/game/systems/SelectionSystem.hpp"

namespace rts::core::model {
    class Building;
    class IGameElement;
    class Unit;
    struct UnitOrder;
    enum class BuildingType;
}

namespace rts::core::world {
    class GameWorld;
}

namespace rts::core::data {
    struct BuildingStaticData;
}

namespace rts::core::manager {
    class GameLogicManager final : public ILogicManager {
    public:
        GameLogicManager(command::LogicCommandBus &bus,
                         command::LogicCommandRouter &router,
                        core::world::GameWorld &world
                         );

        void update() override;

        void tick(float dt) override;

        bool canMoveUnitTo(const model::Unit &unit, const model::Vector2D &pos) const;


        void addSelectedElement(model::IGameElement &element);
        // Logic 전용 API
        void clearSelection();

        void selectElement(model::IGameElement &element);
        void handleMoveCommand(const command::MoveCommand& cmd);
        void handleAttackCommand(const command::AttackCommand& cmd);
        void handleAttackMoveCommand(const command::AttackMoveCommand& cmd);
        void handlePatrolCommand(const command::PatrolCommand& cmd);
        void handleGatherCommand(const command::GatherCommand& cmd);
        void handleTrainCommand(const command::TrainUnitCommand& cmd);
        void handleCancelProduction(const command::CancelProductionCommand& cmd);
        void handleBuildCommand(const command::BuildCommand& cmd);

    private:
        // Units produced this tick are buffered here and flushed after the element
        // sweep, since spawning mid-iteration would invalidate the elements vector.
        struct PendingSpawn {
            ::rts::UnitType type;
            model::Vector2D anchor;
            model::Vector2D rally;
            bool hasRally;
            int team;
        };

        enum class AiBuildOrderState {
            Opening,
            Gather,
            BuildBarracks,
            ProduceArmy,
            Attack,
            Rebuild
        };

        struct ElementSnapshot {
            float hp { 0.0f };
            bool complete { false };
            model::ActionType action { model::ActionType::Idle };
        };

        std::shared_ptr<model::IGameElement> findCommandTargetAt(
            const model::Vector2D& target,
            const SelectionSystem::SelectedList& selected) const;
        std::shared_ptr<model::Building> findClosestDropOffFor(const model::Unit& worker) const;
        std::shared_ptr<model::ResourceNode> findClosestAvailableResource(
            model::ResourceNode::ResourceType type, const model::Unit& requester) const;
        void issueGatherToResource(model::ResourceNode& resource);
        void applyReadyResourceDeliveries();
        void handleGatherRedirects();
        std::shared_ptr<model::IGameElement> findClosestAttackMoveTarget(const model::Unit& unit) const;
        void handleAttackMoveOrders();
        void handleAttackRetargets();
        std::shared_ptr<model::IGameElement> findClosestHoldTarget(const model::Unit& unit) const;
        void handleHoldPositionOrders();
        std::shared_ptr<model::IGameElement> findClosestPatrolTarget(const model::Unit& unit) const;
        void handlePatrolOrders();
        void clearSelectedUnitOrderQueues();
        void queueMoveOrderForSelected(const model::Vector2D& target);
        // Appends one order to every selected unit's queue (workersOnly skips
        // non-workers, e.g. for Gather), starting it immediately if the unit is idle.
        void enqueueOrderForSelected(const model::UnitOrder& order, bool workersOnly);
        void issueNextQueuedOrder(model::Unit& unit);
        void handleQueuedOrders();

        // Production helpers
        void registerBuildingSpawn(model::Building& building);
        std::shared_ptr<model::Building> firstSelectedBuilding() const;
        // Nearest free tile around the anchor; when a preferred world point is given
        // (the rally point), the closest free tile in that direction wins. Returns
        // nullopt when every tile in range is blocked so the caller can wait.
        std::optional<model::Vector2D> findFreeSpawnPosition(
            const model::Vector2D& anchor,
            const std::optional<model::Vector2D>& prefer = std::nullopt) const;
        // True when a live opposing-team element sits near the point (rally on foe).
        bool isEnemyNear(const model::Vector2D& point, int team) const;
        void flushPendingSpawns();
        static ::rts::UnitType defaultUnitFor(model::BuildingType type);

        // Construction helpers
        std::shared_ptr<model::Unit> firstSelectedWorker() const;
        // Checks the whole w x h footprint (top-left origin in tiles) is walkable and free.
        bool canPlaceBuilding(int originX, int originY, int w, int h) const;
        // True when every building type in the static data's requirements list is
        // present and completed for the given team.
        bool hasBuildingRequirements(int teamId, const data::BuildingStaticData& data) const;
        // Snapshots a team's completed buildings (and researched upgrades) for the
        // TechTreeValidator.
        core::tech::TechState buildTechState(int teamId) const;
        // Recomputes each team's food capacity/usage and army size from live units,
        // buildings and training queues, then emits a debug log on any change.
        void recomputeSupply();
        // Logs gold/wood/food/army deltas for a team when they change (debug output
        // for the resource-change event).
        void logResourceChange(int teamId, const core::model::PlayerResourceState& current);

        // Match lifecycle
        void setupInitialWorld(const std::string& mapPath);  // spawns map data (caller holds the lock)
        void restartMatch();       // resets the world and repopulates the starting position

        // Save / load the live match to a JSON snapshot (tick, economy, every
        // unit/building/resource's major state). Returns false on I/O/parse failure.
        bool saveGame(const std::string& path);
        bool loadGame(const std::string& path);
        // Fixed quick-save slot under the data root.
        static std::string quickSavePath();
        static std::string defaultMapPath();

        // Replay control. Recording taps every player command (stamped with the tick
        // it applies on); playback restarts the match and re-dispatches that stream.
        enum class ReplayMode { Off, Record, Play };
        static std::string replayPath();
        void toggleRecording();   // start recording, or stop and write the replay file
        void startReplay();       // load the replay, reset to the start, and play it back
        void startReplayFrom(const std::string& path);  // play a specific replay file
        // Begins recording the current (already set up) match from tick 0.
        void beginRecording();
        // Saves the recorded replay to AppData with a timestamped name (once per match).
        void saveReplayToAppData();
        // Gate + record a live player command: returns false when it should be
        // ignored (live input during playback), records it while in Record mode.
        bool acceptPlayerCommand(const command::LogicCommand& cmd);
        // Re-dispatches the recorded commands stamped for a tick (playback).
        void applyReplayCommands(std::uint64_t tick);

        // AI / victory helpers
        void updateAI(float dt);
        void updateAiBuildOrder();
        // Enemy economy: train workers (to a cap) and warriors, paying from the
        // enemy resource pool (no free units).
        void updateAiProduction(float dt);
        // Sends idle enemy workers to gather the nearest available resource.
        void updateAiWorkers(float dt);
        void updateAiDefense(float dt);
        // Launches an attack wave once enough idle soldiers have massed, or after a
        // timeout, whichever comes first.
        void updateAiWaves(float dt);
        void checkVictoryDefeat();
        std::shared_ptr<model::Building> findTownHall(int teamId) const;
        std::shared_ptr<model::Building> findBarracks(int teamId, bool requireComplete) const;
        std::shared_ptr<model::Unit> findIdleWorker(int teamId) const;
        int countCombatUnits(int teamId, bool idleOnly) const;
        int countTownHalls(int teamId) const;
        model::Vector2D aiRallyPoint() const;
        bool tryStartAiBarracks();
        std::optional<model::Vector2D> findBuildSiteNear(const model::Vector2D& center,
                                                         const data::BuildingStaticData& data) const;
        void emitStateFeedback();
        bool inputLocked() const;
        void captureFeedbackSnapshots();

        core::world::GameWorld& m_world;
        SelectionSystem m_selection;
        ControlGroupSystem m_controlGroups;
        CollisionSystem m_collision;
        MovementSystem m_movement;
        std::vector<PendingSpawn> m_pendingSpawns;

        // Simple enemy AI: trains/gathers on cadences and sends idle combat units
        // at the player's town hall once they mass up.
        AiBuildOrderState m_aiState { AiBuildOrderState::Opening };
        float m_aiProduceTimer { 0.f };
        float m_aiGatherTimer { 0.f };
        float m_aiWaveTimer { 0.f };
        float m_aiDefenseTimer { 0.f };
        AiBuildOrderState m_lastFeedbackAiState { AiBuildOrderState::Opening };

        // Last logged resource snapshot per team (index by TeamId) for delta logging.
        core::model::PlayerResourceState m_lastResourceLog[3] {};
        bool m_resourceLogReady { false };
        // Keyed by full EntityId (index<<32 | generation) so a recycled slot never
        // mis-attributes one unit's HP change to another (which spawned phantom hits).
        std::unordered_map<std::uint64_t, ElementSnapshot> m_feedbackSnapshots;

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
