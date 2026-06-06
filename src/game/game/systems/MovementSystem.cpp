#include "game/game/systems/MovementSystem.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <optional>
#include <vector>

#include <core/manager/PathManager.hpp>
#include <core/model/IGameElement.hpp>
#include <core/model/Unit.hpp>
#include <core/world/GameWorld.hpp>

#include "game/game/systems/CollisionSystem.hpp"

namespace {
    using rts::core::manager::CollisionSystem;
    using rts::core::model::Unit;
    using rts::core::model::Vector2D;
    using rts::core::world::GameWorld;

    float distanceSq(const Vector2D& a, const Vector2D& b) {
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        return dx * dx + dy * dy;
    }

    bool sameCell(const rts::core::path::GridPos& a, const rts::core::path::GridPos& b) {
        return a.x == b.x && a.y == b.y;
    }

    struct MovePlan {
        rts::core::path::Path path;
        Vector2D finalTarget;
    };

    rts::core::path::PathOptions movementPathOptions() {
        rts::core::path::PathOptions options;
        options.allowDiagonal = true;
        options.useDynamicBlocking = true;
        options.preventDiagonalCornerCutting = true;
        return options;
    }

    bool finalTargetBlockedByHit(
        const Unit& unit,
        const CollisionSystem& collision,
        const CollisionSystem::CollisionHit& hit) {
        if (!hit.hasCenter) {
            return false;
        }

        const float stopDistance = collision.movingUnitRadius() + hit.radius + 1.0f;
        return distanceSq(unit.finalTargetWorld(), hit.center) <= stopDistance * stopDistance;
    }

    std::optional<MovePlan> findApproachPath(
        GameWorld& world,
        const rts::core::path::GridPos& start,
        const rts::core::path::GridPos& goal,
        const Vector2D& requestedTarget,
        const rts::core::path::PathOptions& options) {
        const auto& transform = world.gridTransform();
        std::optional<MovePlan> bestPlan;
        float bestScore = std::numeric_limits<float>::max();

        for (int radius = 1; radius <= 3; ++radius) {
            for (int dy = -radius; dy <= radius; ++dy) {
                for (int dx = -radius; dx <= radius; ++dx) {
                    if (std::max(std::abs(dx), std::abs(dy)) != radius) {
                        continue;
                    }

                    const rts::core::path::GridPos candidate {
                        goal.x + dx,
                        goal.y + dy
                    };

                    if (!world.gridQuery().inBounds(candidate) ||
                        world.gridQuery().isBlockedStatic(candidate) ||
                        world.gridQuery().isBlockedDynamic(candidate)) {
                        continue;
                    }

                    auto path = world.path().findPath(
                        world.gridQuery(),
                        world.collisionVersion(),
                        start,
                        candidate,
                        options
                    );
                    if (!path || path->empty()) {
                        continue;
                    }

                    const Vector2D candidateWorld = transform.gridToWorldCenter(candidate);
                    const float score =
                        static_cast<float>(path->size()) * 64.0f +
                        distanceSq(candidateWorld, requestedTarget) * 0.01f;

                    if (score < bestScore) {
                        bestScore = score;
                        bestPlan = MovePlan{*path, candidateWorld};
                    }
                }
            }

            if (bestPlan.has_value()) {
                break;
            }
        }

        return bestPlan;
    }

    std::optional<rts::core::path::GridPos> findAvoidanceCell(
        GameWorld& world,
        const Unit& unit,
        const Vector2D& safePosition,
        const CollisionSystem& collision,
        const CollisionSystem::CollisionHit& hit) {
        if (!hit.hasCenter) {
            return std::nullopt;
        }

        const auto& transform = world.gridTransform();
        const auto blockerCell = transform.worldToGrid(hit.center);
        const auto safeCell = transform.worldToGrid(safePosition);
        const auto finalTarget = unit.finalTargetWorld();

        std::optional<rts::core::path::GridPos> bestCell;
        float bestScore = std::numeric_limits<float>::max();

        for (int radius = 1; radius <= 3; ++radius) {
            for (int dy = -radius; dy <= radius; ++dy) {
                for (int dx = -radius; dx <= radius; ++dx) {
                    if (std::max(std::abs(dx), std::abs(dy)) != radius) {
                        continue;
                    }

                    const rts::core::path::GridPos candidate {
                        blockerCell.x + dx,
                        blockerCell.y + dy
                    };

                    if (sameCell(candidate, safeCell) ||
                        !world.gridQuery().inBounds(candidate) ||
                        world.gridQuery().isBlockedStatic(candidate)) {
                        continue;
                    }

                    const auto candidateWorld = transform.gridToWorldCenter(candidate);
                    if (!collision.canMoveUnitTo(world, unit, candidateWorld)) {
                        continue;
                    }

                    // Prefer a nearby side-step that still keeps the unit moving toward the clicked target.
                    const float score =
                        distanceSq(safePosition, candidateWorld) * 0.7f +
                        distanceSq(candidateWorld, finalTarget) * 0.3f;

                    if (score < bestScore) {
                        bestScore = score;
                        bestCell = candidate;
                    }
                }
            }

            if (bestCell.has_value()) {
                break;
            }
        }

        return bestCell;
    }

    bool issueAvoidancePath(
        GameWorld& world,
        Unit& unit,
        const Vector2D& safePosition,
        const CollisionSystem& collision,
        const CollisionSystem::CollisionHit& hit) {
        const auto avoidCell = findAvoidanceCell(world, unit, safePosition, collision, hit);
        if (!avoidCell) {
            return false;
        }

        const auto& transform = world.gridTransform();
        const auto startCell = transform.worldToGrid(safePosition);
        const auto goalCell = transform.worldToGrid(unit.finalTargetWorld());

        rts::core::path::Path path;
        path.push_back(startCell);
        path.push_back(*avoidCell);

        auto options = movementPathOptions();

        if (auto tail = world.path().findPath(
                world.gridQuery(),
                world.collisionVersion(),
                *avoidCell,
                goalCell,
                options)) {
            path.insert(path.end(), std::next(tail->begin()), tail->end());
        } else {
            return false;
        }

        unit.setMoveTargetWithPath(path, unit.finalTargetWorld());
        return true;
    }
}

namespace rts::core::manager {
    void MovementSystem::update(
        world::GameWorld& world,
        float dt,
        const CollisionSystem& collision) const {
        for (const auto& element : world.getElements()) {
            if (auto unit = std::dynamic_pointer_cast<model::Unit>(element)) {
                const auto previousPosition = unit->getPosition();
                if (unit->getAction() == model::ActionType::Move) {
                    unit->updateMove(dt, world.gridTransform());
                } else {
                    unit->tick(dt);
                }

                // Movement is speculative until the collision system accepts the next position.
                if (unit->getAction() != model::ActionType::Dead) {
                    const auto hit = collision.findMoveBlocker(world, *unit, unit->getPosition());
                    if (hit.has_value()) {
                        unit->setPosition(previousPosition);
                        if (finalTargetBlockedByHit(*unit, collision, *hit)) {
                            unit->stop();
                            continue;
                        }

                        if (!issueAvoidancePath(world, *unit, previousPosition, collision, *hit)) {
                            unit->stop();
                        }
                    }
                }

                const auto currentPosition = unit->getPosition();
                if (currentPosition.x != previousPosition.x ||
                    currentPosition.y != previousPosition.y) {
                    world.onCollisionChanged();
                }
                continue;
            }

            if (auto gameElement = std::dynamic_pointer_cast<model::IGameElement>(element)) {
                gameElement->tick(dt);
            }
        }
    }

    void MovementSystem::issueMove(
        world::GameWorld& world,
        const SelectionSystem::SelectedList& selected,
        const model::Vector2D& target) const {
        const auto& transform = world.gridTransform();
        const auto goal = transform.worldToGrid(target);

        for (const auto& weak : selected) {
            if (auto element = weak.lock()) {
                if (element->getAction() == model::ActionType::Dead) {
                    continue;
                }

                auto unit = std::dynamic_pointer_cast<model::Unit>(element);
                if (!unit) {
                    element->stop();
                    element->moveTo(target);
                    continue;
                }

                auto options = movementPathOptions();

                const auto start = transform.worldToGrid(unit->getPosition());
                const auto path = world.path().findPath(
                    world.gridQuery(),
                    world.collisionVersion(),
                    start,
                    goal,
                    options
                );

                unit->stop();
                if (path && !path->empty()) {
                    unit->setMoveTargetWithPath(*path, target);
                } else if (auto approach = findApproachPath(world, start, goal, target, options)) {
                    // Occupied goals are approached through a nearby free cell instead of retrying forever.
                    unit->setMoveTargetWithPath(approach->path, approach->finalTarget);
                }
            }
        }
    }
}
