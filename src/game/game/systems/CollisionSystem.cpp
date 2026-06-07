#include "game/game/systems/CollisionSystem.hpp"

#include <core/model/Building.hpp>
#include <core/model/ResourceNode.hpp>
#include <core/model/Unit.hpp>
#include <core/world/GameWorld.hpp>

namespace {
    constexpr float kMapMinX = 0.f;
    constexpr float kMapMinY = 0.f;
    constexpr float kMapMaxX = 2000.f;
    constexpr float kMapMaxY = 2000.f;
    constexpr float kUnitCollisionRadius = 28.f;
    constexpr float kResourceCollisionRadius = 44.f;
    constexpr float kBuildingCollisionRadius = 52.f;

    float collisionRadiusFor(const rts::core::model::IGameElement& element) {
        if (dynamic_cast<const rts::core::model::Unit*>(&element)) {
            return kUnitCollisionRadius;
        }
        if (dynamic_cast<const rts::core::model::ResourceNode*>(&element)) {
            return kResourceCollisionRadius;
        }
        if (dynamic_cast<const rts::core::model::Building*>(&element)) {
            // Keep this below current gather/build interaction ranges so workers can
            // still stop at the edge instead of being cancelled by collision.
            return kBuildingCollisionRadius;
        }
        return kUnitCollisionRadius;
    }
}

namespace rts::core::manager {
    bool CollisionSystem::canMoveUnitTo(
        const world::GameWorld& world,
        const model::Unit& unit,
        const model::Vector2D& pos) const {
        return !findMoveBlocker(world, unit, pos).has_value();
    }

    std::optional<CollisionSystem::CollisionHit> CollisionSystem::findMoveBlocker(
        const world::GameWorld& world,
        const model::Unit& unit,
        const model::Vector2D& pos) const {
        if (pos.x < kMapMinX || pos.y < kMapMinY ||
            pos.x > kMapMaxX || pos.y > kMapMaxY) {
            return CollisionHit {};
        }

        for (const auto& element : world.getElements()) {
            const auto other = std::dynamic_pointer_cast<model::IGameElement>(element);
            if (!other ||
                other.get() == static_cast<const model::IGameElement*>(&unit) ||
                other->getAction() == model::ActionType::Dead) {
                continue;
            }

            const auto otherPosition = other->getPosition();
            const float dx = otherPosition.x - pos.x;
            const float dy = otherPosition.y - pos.y;
            const float minDistance = movingUnitRadius() + collisionRadiusFor(*other);

            if ((dx * dx + dy * dy) < minDistance * minDistance) {
                return CollisionHit {
                    otherPosition,
                    collisionRadiusFor(*other),
                    true
                };
            }
        }

        return std::nullopt;
    }

    float CollisionSystem::movingUnitRadius() const noexcept {
        return kUnitCollisionRadius;
    }
}
