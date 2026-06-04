#pragma once

namespace rts::core::model {
    class Unit;
    struct Vector2D;
}

namespace rts::core::world {
    class GameWorld;
}

namespace rts::core::manager {
    class CollisionSystem {
    public:
        bool canMoveUnitTo(const world::GameWorld& world,
                           const model::Unit& unit,
                           const model::Vector2D& pos) const;
    };
}
