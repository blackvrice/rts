#include "game/game/systems/CollisionSystem.hpp"

#include <core/model/Unit.hpp>
#include <core/world/GameWorld.hpp>

namespace {
    constexpr float kMapMinX = 0.f;
    constexpr float kMapMinY = 0.f;
    constexpr float kMapMaxX = 2000.f;
    constexpr float kMapMaxY = 2000.f;
    constexpr float kUnitCollisionRadius = 28.f;
    constexpr float kMinUnitDistanceSq = kUnitCollisionRadius * kUnitCollisionRadius * 4.f;
}

namespace rts::core::manager {
    bool CollisionSystem::canMoveUnitTo(
        const world::GameWorld& world,
        const model::Unit& unit,
        const model::Vector2D& pos) const {
        if (pos.x < kMapMinX || pos.y < kMapMinY ||
            pos.x > kMapMaxX || pos.y > kMapMaxY) {
            return false;
        }

        for (const auto& element : world.getElements()) {
            const auto other = std::dynamic_pointer_cast<model::Unit>(element);
            if (!other || other.get() == &unit) {
                continue;
            }

            const auto otherPosition = other->getPosition();
            const float dx = otherPosition.x - pos.x;
            const float dy = otherPosition.y - pos.y;

            if ((dx * dx + dy * dy) < kMinUnitDistanceSq) {
                return false;
            }
        }

        return true;
    }
}
