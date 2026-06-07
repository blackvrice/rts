#pragma once

#include "game/game/systems/SelectionSystem.hpp"

namespace rts::core::model {
    class Unit;
    struct Vector2D;
}

namespace rts::core::world {
    class GameWorld;
}

namespace rts::core::manager {
    class CollisionSystem;

    class MovementSystem {
    public:
        void update(world::GameWorld& world, float dt, const CollisionSystem& collision) const;
        void issueMove(world::GameWorld& world,
                       const SelectionSystem::SelectedList& selected,
                       const model::Vector2D& target,
                       bool clearQueuedOrders = true) const;
        void issueMove(world::GameWorld& world,
                       model::Unit& unit,
                       const model::Vector2D& target,
                       bool clearQueuedOrders = true) const;
        void issueAttackMove(world::GameWorld& world,
                             const SelectionSystem::SelectedList& selected,
                             const model::Vector2D& target) const;
        void issueAttackMove(world::GameWorld& world,
                             model::Unit& unit,
                             const model::Vector2D& target) const;
        void issuePatrol(world::GameWorld& world,
                         const SelectionSystem::SelectedList& selected,
                         const model::Vector2D& target) const;
        void issuePatrol(world::GameWorld& world,
                         model::Unit& unit,
                         const model::Vector2D& target) const;
    };
}
