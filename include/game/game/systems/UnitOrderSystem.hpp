#pragma once

#include "core/model/Vector2D.hpp"
#include "game/game/systems/SelectionSystem.hpp"

namespace rts::core::command {
    class AttackCommand;
    class AttackMoveCommand;
    class GatherCommand;
    class MoveCommand;
    class PatrolCommand;
}

namespace rts::core::model {
    class Unit;
    struct UnitOrder;
}

namespace rts::core::world {
    class GameWorld;
}

namespace rts::core::manager {
    class EconomySystem;
    class MovementSystem;

    // Player orders for the selected units (move, smart right-click, attack-move,
    // patrol, gather, stop, hold) and Shift-queued order chains. Callers hold the
    // world write lock and have already rejected input (replay / match over).
    class UnitOrderSystem {
    public:
        UnitOrderSystem(MovementSystem& movement, EconomySystem& economy)
            : m_movement(movement), m_economy(economy) {}

        void move(world::GameWorld& world, const SelectionSystem::SelectedList& selected,
                  const command::MoveCommand& cmd);
        // Smart right-click: resource -> gather, enemy -> attack, friendly or
        // empty ground -> move. Shift appends to the order queue instead.
        void attack(world::GameWorld& world, const SelectionSystem::SelectedList& selected,
                    const command::AttackCommand& cmd);
        void attackMove(world::GameWorld& world, const SelectionSystem::SelectedList& selected,
                        const command::AttackMoveCommand& cmd);
        void patrol(world::GameWorld& world, const SelectionSystem::SelectedList& selected,
                    const command::PatrolCommand& cmd);
        void gather(world::GameWorld& world, const SelectionSystem::SelectedList& selected,
                    const command::GatherCommand& cmd);
        void stop(const SelectionSystem::SelectedList& selected);
        void hold(const SelectionSystem::SelectedList& selected);

        // Starts the next queued order for every idle unit that has one (tick pass).
        void handleQueuedOrders(world::GameWorld& world);

    private:
        void clearSelectedUnitOrderQueues(const SelectionSystem::SelectedList& selected);
        void queueMoveOrderForSelected(world::GameWorld& world,
                                       const SelectionSystem::SelectedList& selected,
                                       const model::Vector2D& target);
        // Appends one order to every selected unit's queue (workersOnly skips
        // non-workers, e.g. for Gather), starting it immediately if the unit is idle.
        void enqueueOrderForSelected(world::GameWorld& world,
                                     const SelectionSystem::SelectedList& selected,
                                     const model::UnitOrder& order, bool workersOnly);
        void issueNextQueuedOrder(world::GameWorld& world, model::Unit& unit);

        MovementSystem& m_movement;
        EconomySystem& m_economy;
    };
}
