#pragma once

#include <optional>

#include "core/model/Vector2D.hpp"

namespace rts::core::model {
    class Unit;
}

namespace rts::core::world {
    class GameWorld;
}

namespace rts::core::manager {
    class CollisionSystem {
    public:
        struct CollisionHit {
            model::Vector2D center {};
            float radius {};
            bool hasCenter {};
            bool isUnit {};
        };

        bool canMoveUnitTo(const world::GameWorld& world,
                           const model::Unit& unit,
                           const model::Vector2D& pos) const;

        std::optional<CollisionHit> findMoveBlocker(const world::GameWorld& world,
                                                    const model::Unit& unit,
                                                    const model::Vector2D& pos) const;

        std::optional<model::Vector2D> localAvoidancePush(const world::GameWorld& world,
                                                          const model::Unit& unit,
                                                          const model::Vector2D& pos) const;

        bool canApplyUnitSeparationTo(const world::GameWorld& world,
                                      const model::Unit& unit,
                                      const model::Vector2D& pos) const;

        float movingUnitRadius() const noexcept;
    };
}
