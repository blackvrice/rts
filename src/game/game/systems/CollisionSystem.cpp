#include "game/game/systems/CollisionSystem.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

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
    constexpr float kMaxLocalAvoidancePush = 8.f;
    constexpr float kOverlapPadding = 0.5f;
    constexpr float kTinyDistanceSq = 0.0001f;

    float collisionRadiusFor(const rts::core::model::IGameElement& element) {
        if (const auto* unit = dynamic_cast<const rts::core::model::Unit*>(&element)) {
            return unit->getCollisionRadius();
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

    std::size_t elementOrdinal(
        const rts::core::world::GameWorld& world,
        const rts::core::model::IGameElement& target) {
        std::size_t ordinal = 0;
        for (const auto& element : world.getElements()) {
            const auto candidate = std::dynamic_pointer_cast<rts::core::model::IGameElement>(element);
            if (candidate && candidate.get() == &target) {
                return ordinal;
            }
            ++ordinal;
        }

        return ordinal;
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
            const float minDistance = unit.getCollisionRadius() + collisionRadiusFor(*other);

            if ((dx * dx + dy * dy) < minDistance * minDistance) {
                const bool blockerIsUnit = dynamic_cast<const model::Unit*>(other.get()) != nullptr;
                return CollisionHit {
                    otherPosition,
                    collisionRadiusFor(*other),
                    true,
                    blockerIsUnit
                };
            }
        }

        return std::nullopt;
    }

    std::optional<model::Vector2D> CollisionSystem::localAvoidancePush(
        const world::GameWorld& world,
        const model::Unit& unit,
        const model::Vector2D& pos) const {
        model::Vector2D push {};
        const auto selfOrdinal = elementOrdinal(world, unit);

        std::size_t otherOrdinal = 0;
        for (const auto& element : world.getElements()) {
            const auto otherUnit = std::dynamic_pointer_cast<model::Unit>(element);
            if (!otherUnit ||
                otherUnit.get() == &unit ||
                otherUnit->getAction() == model::ActionType::Dead) {
                ++otherOrdinal;
                continue;
            }

            const auto otherPosition = otherUnit->getPosition();
            const float dx = pos.x - otherPosition.x;
            const float dy = pos.y - otherPosition.y;
            const float minDistance = unit.getCollisionRadius() + otherUnit->getCollisionRadius();
            const float distSq = dx * dx + dy * dy;
            if (distSq >= minDistance * minDistance) {
                ++otherOrdinal;
                continue;
            }

            float dist = std::sqrt(distSq);
            model::Vector2D away {};
            if (distSq > kTinyDistanceSq) {
                away = { dx / dist, dy / dist };
            } else {
                // Fully stacked units need a deterministic fallback direction until
                // stable EntityId ordering exists; world insertion order is stable here.
                away = otherOrdinal < selfOrdinal
                    ? model::Vector2D { 1.0f, 0.0f }
                    : model::Vector2D { -1.0f, 0.0f };
                dist = 0.0f;
            }

            const float overlap = std::max(0.0f, minDistance - dist + kOverlapPadding);
            push = push + away * overlap;
            ++otherOrdinal;
        }

        const float pushLenSq = push.x * push.x + push.y * push.y;
        if (pushLenSq <= kTinyDistanceSq) {
            return std::nullopt;
        }

        const float pushLen = std::sqrt(pushLenSq);
        if (pushLen > kMaxLocalAvoidancePush) {
            push = push * (kMaxLocalAvoidancePush / pushLen);
        }

        return push;
    }

    bool CollisionSystem::canApplyUnitSeparationTo(
        const world::GameWorld& world,
        const model::Unit& unit,
        const model::Vector2D& pos) const {
        if (pos.x < kMapMinX || pos.y < kMapMinY ||
            pos.x > kMapMaxX || pos.y > kMapMaxY) {
            return false;
        }

        for (const auto& element : world.getElements()) {
            const auto other = std::dynamic_pointer_cast<model::IGameElement>(element);
            if (!other ||
                other.get() == static_cast<const model::IGameElement*>(&unit) ||
                other->getAction() == model::ActionType::Dead ||
                std::dynamic_pointer_cast<model::Unit>(element)) {
                continue;
            }

            const auto otherPosition = other->getPosition();
            const float dx = otherPosition.x - pos.x;
            const float dy = otherPosition.y - pos.y;
            const float minDistance = unit.getCollisionRadius() + collisionRadiusFor(*other);
            if ((dx * dx + dy * dy) < minDistance * minDistance) {
                return false;
            }
        }

        return true;
    }

    float CollisionSystem::movingUnitRadius() const noexcept {
        return kUnitCollisionRadius;
    }
}
