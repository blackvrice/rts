//
// Created by black on 25. 12. 25..
//

#pragma once
#include <cstdint>
#include <memory>
#include <vector>

#include "core/manager/ILogicManager.hpp"
#include "core/model/ResourceNode.hpp"
#include "core/model/UnitType.hpp"
#include "core/model/Vector2D.hpp"
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
        model::Vector2D findFreeSpawnPosition(const model::Vector2D& anchor) const;
        void flushPendingSpawns();
        static ::rts::UnitType defaultUnitFor(model::BuildingType type);

        // Construction helpers
        std::shared_ptr<model::Unit> firstSelectedWorker() const;
        // Checks the whole w x h footprint (top-left origin in tiles) is walkable and free.
        bool canPlaceBuilding(int originX, int originY, int w, int h) const;
        // True when every building type in the static data's requirements list is
        // present and completed for the given team.
        bool hasBuildingRequirements(int teamId, const data::BuildingStaticData& data) const;
        // Recomputes each team's food capacity from the providesSupply of their
        // completed buildings.
        void recomputeSupply();

        // AI / victory helpers
        void updateAI(float dt);
        // Enemy economy: train workers (to a cap) and warriors, paying from the
        // enemy resource pool (no free units).
        void updateAiProduction(float dt);
        // Sends idle enemy workers to gather the nearest available resource.
        void updateAiWorkers(float dt);
        // Launches an attack wave once enough idle soldiers have massed, or after a
        // timeout, whichever comes first.
        void updateAiWaves(float dt);
        void checkVictoryDefeat();
        std::shared_ptr<model::Building> findTownHall(int teamId) const;
        int countTownHalls(int teamId) const;
        bool inputLocked() const;

        core::world::GameWorld& m_world;
        SelectionSystem m_selection;
        ControlGroupSystem m_controlGroups;
        CollisionSystem m_collision;
        MovementSystem m_movement;
        std::vector<PendingSpawn> m_pendingSpawns;

        // Simple enemy AI: trains/gathers on cadences and sends idle combat units
        // at the player's town hall once they mass up.
        float m_aiProduceTimer { 0.f };
        float m_aiGatherTimer { 0.f };
        float m_aiWaveTimer { 0.f };
    };
} // namespace rts::manager
