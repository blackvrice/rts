#include "game/game/systems/CollisionSystem.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include <core/data/BuildingStaticData.hpp>
#include <core/data/DataRegistry.hpp>
#include <core/data/ResourceStaticData.hpp>
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

    // Axis-aligned footprint rectangle (world units) of a static structure,
    // centered on its position. Returns false for units (which stay circles).
    struct FootprintRect { float cx, cy, hx, hy; };
    bool structureRect(const rts::core::model::IGameElement& element,
                       const rts::core::world::GameWorld& world,
                       FootprintRect& out) {
        const float tile = world.gridTransform().tileSize;
        int fw = 0;
        int fh = 0;
        if (const auto* b = dynamic_cast<const rts::core::model::Building*>(&element)) {
            const auto& d = rts::core::data::DataRegistry::global().building(b->buildingType());
            fw = d.footprintWidth;
            fh = d.footprintHeight;
        } else if (const auto* r = dynamic_cast<const rts::core::model::ResourceNode*>(&element)) {
            const auto& d = rts::core::data::DataRegistry::global().resource(r->type());
            fw = d.footprintWidth;
            fh = d.footprintHeight;
        } else {
            return false;
        }
        const auto c = element.getPosition();
        out = { c.x, c.y, static_cast<float>(fw) * tile * 0.5f, static_cast<float>(fh) * tile * 0.5f };
        return true;
    }

    // True when a circle (center p, radius r) overlaps the footprint rectangle;
    // outputs the closest point on the rectangle for the collision response.
    bool circleHitsRect(const rts::core::model::Vector2D& p, const float r,
                        const FootprintRect& box, rts::core::model::Vector2D& nearest) {
        const float nx = std::clamp(p.x, box.cx - box.hx, box.cx + box.hx);
        const float ny = std::clamp(p.y, box.cy - box.hy, box.cy + box.hy);
        nearest = { nx, ny };
        const float dx = p.x - nx;
        const float dy = p.y - ny;
        return (dx * dx + dy * dy) < r * r;
    }

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

            // Static structures block as their footprint rectangle; units as circles.
            FootprintRect box;
            if (structureRect(*other, world, box)) {
                model::Vector2D nearest;
                if (circleHitsRect(pos, unit.getCollisionRadius(), box, nearest)) {
                    return CollisionHit { nearest, 0.0f, true, false };
                }
                continue;
            }

            const auto otherPosition = other->getPosition();
            const float dx = otherPosition.x - pos.x;
            const float dy = otherPosition.y - pos.y;
            const float minDistance = unit.getCollisionRadius() + collisionRadiusFor(*other);

            if ((dx * dx + dy * dy) < minDistance * minDistance) {
                return CollisionHit {
                    otherPosition,
                    collisionRadiusFor(*other),
                    true,
                    true  // the only non-structure blocker is another unit
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

            // Non-unit blockers are static structures: test against the footprint rect.
            FootprintRect box;
            if (structureRect(*other, world, box)) {
                model::Vector2D nearest;
                if (circleHitsRect(pos, unit.getCollisionRadius(), box, nearest)) {
                    return false;
                }
            }
        }

        return true;
    }

    std::optional<model::Vector2D> CollisionSystem::structureEscapePush(
        const world::GameWorld& world,
        const model::Unit& unit,
        const model::Vector2D& pos) const {
        const float ur = unit.getCollisionRadius();
        model::Vector2D push {};
        bool overlapped = false;

        for (const auto& element : world.getElements()) {
            const auto other = std::dynamic_pointer_cast<model::IGameElement>(element);
            if (!other || other->getAction() == model::ActionType::Dead) {
                continue;
            }
            FootprintRect box;
            if (!structureRect(*other, world, box)) {
                continue;  // only static structures evict units
            }

            // Only evict a unit whose CENTER is inside the footprint (genuinely
            // stuck, e.g. a building placed on top of it). A unit merely touching the
            // edge is left alone so workers can gather / drop off and melee units can
            // stand adjacent to a structure.
            if (pos.x <= box.cx - box.hx || pos.x >= box.cx + box.hx ||
                pos.y <= box.cy - box.hy || pos.y >= box.cy + box.hy) {
                continue;
            }
            overlapped = true;

            // Exit along the axis of least penetration, clearing the unit's radius.
            const float left = pos.x - (box.cx - box.hx);
            const float right = (box.cx + box.hx) - pos.x;
            const float top = pos.y - (box.cy - box.hy);
            const float bottom = (box.cy + box.hy) - pos.y;
            const float minX = std::min(left, right);
            const float minY = std::min(top, bottom);
            if (minX < minY) {
                push.x += (left < right ? -(left + ur) : (right + ur));
            } else {
                push.y += (top < bottom ? -(top + ur) : (bottom + ur));
            }
        }

        if (!overlapped) {
            return std::nullopt;
        }
        return push;
    }

    float CollisionSystem::movingUnitRadius() const noexcept {
        return kUnitCollisionRadius;
    }
}
