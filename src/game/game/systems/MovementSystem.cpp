#include "game/game/systems/MovementSystem.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
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
    using rts::core::manager::PathOrderKind;
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

    const char* pathOrderKindName(const PathOrderKind kind) {
        switch (kind) {
            case PathOrderKind::Move:
                return "Move";
            case PathOrderKind::AttackMove:
                return "AttackMove";
            case PathOrderKind::Patrol:
                return "Patrol";
        }

        return "Unknown";
    }

    rts::core::path::PathOptions movementPathOptions() {
        rts::core::path::PathOptions options;
        // Gameplay movement allows diagonals for natural paths, but PathManager still
        // rejects diagonal corner-cutting through blocked side cells.
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

        if (unit.isAttackMoveActive()) {
            unit.setAttackMoveTargetWithPath(path, unit.finalTargetWorld());
        } else if (unit.isPatrolActive()) {
            unit.setPatrolTargetWithPath(path, unit.finalTargetWorld());
        } else {
            unit.setMoveTargetWithPath(path, unit.finalTargetWorld());
        }
        return true;
    }

    const char* pathFailureReason(
        GameWorld& world,
        const rts::core::path::GridPos& start,
        const rts::core::path::GridPos& goal) {
        const auto& grid = world.gridQuery();
        if (!grid.inBounds(start)) {
            return "start_out_of_bounds";
        }
        if (!grid.inBounds(goal)) {
            return "goal_out_of_bounds";
        }
        if (grid.isBlockedStatic(goal)) {
            return "goal_static_blocked";
        }
        if (grid.isBlockedDynamic(goal)) {
            return "goal_dynamic_blocked";
        }
        return "no_path";
    }

    void logPathFailure(
        const PathOrderKind kind,
        const rts::core::path::GridPos& start,
        const rts::core::path::GridPos& goal,
        const Vector2D& target,
        const char* reason) {
        std::cerr << "[MovementSystem] Path order failed"
                  << " kind=" << pathOrderKindName(kind)
                  << " reason=" << reason
                  << " start=(" << start.x << "," << start.y << ")"
                  << " goal=(" << goal.x << "," << goal.y << ")"
                  << " target=(" << target.x << "," << target.y << ")"
                  << '\n';
    }

    bool issuePathOrder(
        GameWorld& world,
        Unit& unit,
        const Vector2D& target,
        PathOrderKind kind,
        std::optional<Vector2D> patrolStart = std::nullopt) {
        const auto& transform = world.gridTransform();
        const Vector2D from = patrolStart.value_or(unit.getPosition());
        const auto start = transform.worldToGrid(unit.getPosition());
        const auto goal = transform.worldToGrid(target);
        auto options = movementPathOptions();

        const auto path = world.path().findPath(
            world.gridQuery(),
            world.collisionVersion(),
            start,
            goal,
            options
        );

        if (kind != PathOrderKind::Patrol || patrolStart.has_value()) {
            unit.stop();
        }

        bool issued = false;
        const bool canTryApproach =
            world.gridQuery().inBounds(start) &&
            world.gridQuery().inBounds(goal);
        if (path && !path->empty()) {
            switch (kind) {
                case PathOrderKind::Move:
                    unit.setMoveTargetWithPath(*path, target);
                    break;
                case PathOrderKind::AttackMove:
                    unit.setAttackMoveTargetWithPath(*path, target);
                    break;
                case PathOrderKind::Patrol:
                    if (patrolStart.has_value()) {
                        unit.setPatrolRouteWithPath(*path, target, from, target);
                    } else {
                        unit.setPatrolTargetWithPath(*path, target);
                    }
                    break;
            }
            issued = true;
        } else if (auto approach = canTryApproach
                   ? findApproachPath(world, start, goal, target, options)
                   : std::optional<MovePlan>{}) {
            // Occupied goals are approached through a nearby free cell instead of retrying forever.
            switch (kind) {
                case PathOrderKind::Move:
                    unit.setMoveTargetWithPath(approach->path, approach->finalTarget);
                    break;
                case PathOrderKind::AttackMove:
                    unit.setAttackMoveTargetWithPath(approach->path, approach->finalTarget);
                    break;
                case PathOrderKind::Patrol:
                    if (patrolStart.has_value()) {
                        unit.setPatrolRouteWithPath(approach->path, approach->finalTarget, from, target);
                    } else {
                        unit.setPatrolTargetWithPath(approach->path, approach->finalTarget);
                    }
                    break;
            }
            issued = true;
        }

        if (!issued) {
            const char* reason = pathFailureReason(world, start, goal);
            logPathFailure(kind, start, goal, target, reason);
            unit.stop();
        }

        return issued;
    }
}

namespace rts::core::manager {
    void MovementSystem::update(
        world::GameWorld& world,
        float dt,
        const CollisionSystem& collision) {
        processQueuedPathRequests(world);

        for (const auto& element : world.getElements()) {
            if (auto unit = std::dynamic_pointer_cast<model::Unit>(element)) {
                const auto previousPosition = unit->getPosition();
                if (unit->getAction() == model::ActionType::Move ||
                    unit->getAction() == model::ActionType::Patrol) {
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
        const model::Vector2D& target,
        bool clearQueuedOrders) {
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

                if (clearQueuedOrders) {
                    unit->clearOrderQueue();
                }
                enqueuePathRequest(world, unit, target, PathOrderKind::Move);
            }
        }
    }

    void MovementSystem::issueMove(
        world::GameWorld& world,
        model::Unit& unit,
        const model::Vector2D& target,
        bool clearQueuedOrders) {
        if (unit.getAction() == model::ActionType::Dead) {
            return;
        }

        if (clearQueuedOrders) {
            unit.clearOrderQueue();
        }
        if (auto handle = findUnitHandle(world, unit)) {
            enqueuePathRequest(world, handle, target, PathOrderKind::Move);
        } else {
            issuePathOrder(world, unit, target, PathOrderKind::Move);
        }
    }

    void MovementSystem::issueAttackMove(
        world::GameWorld& world,
        const SelectionSystem::SelectedList& selected,
        const model::Vector2D& target) {
        for (const auto& weak : selected) {
            if (auto unit = std::dynamic_pointer_cast<model::Unit>(weak.lock());
                unit && unit->getAction() != model::ActionType::Dead) {
                enqueuePathRequest(world, unit, target, PathOrderKind::AttackMove);
            }
        }
    }

    void MovementSystem::issueAttackMove(
        world::GameWorld& world,
        model::Unit& unit,
        const model::Vector2D& target) {
        if (unit.getAction() == model::ActionType::Dead) {
            return;
        }

        if (auto handle = findUnitHandle(world, unit)) {
            enqueuePathRequest(world, handle, target, PathOrderKind::AttackMove);
        } else {
            issuePathOrder(world, unit, target, PathOrderKind::AttackMove);
        }
    }

    void MovementSystem::issuePatrol(
        world::GameWorld& world,
        const SelectionSystem::SelectedList& selected,
        const model::Vector2D& target) {
        for (const auto& weak : selected) {
            if (auto unit = std::dynamic_pointer_cast<model::Unit>(weak.lock());
                unit && unit->getAction() != model::ActionType::Dead) {
                enqueuePathRequest(world, unit, target, PathOrderKind::Patrol, unit->getPosition());
            }
        }
    }

    void MovementSystem::issuePatrol(
        world::GameWorld& world,
        model::Unit& unit,
        const model::Vector2D& target) {
        if (unit.getAction() == model::ActionType::Dead) {
            return;
        }

        if (auto handle = findUnitHandle(world, unit)) {
            enqueuePathRequest(world, handle, target, PathOrderKind::Patrol);
        } else {
            issuePathOrder(world, unit, target, PathOrderKind::Patrol);
        }
    }

    void MovementSystem::cancelQueuedPath(model::Unit& unit) {
        m_latestPathRequestByUnit.erase(&unit);
    }

    void MovementSystem::cancelQueuedPaths(const SelectionSystem::SelectedList& selected) {
        for (const auto& weak : selected) {
            if (auto unit = std::dynamic_pointer_cast<model::Unit>(weak.lock())) {
                cancelQueuedPath(*unit);
            }
        }
    }

    std::shared_ptr<model::Unit> MovementSystem::findUnitHandle(
        world::GameWorld& world,
        model::Unit& unit) const {
        for (const auto& element : world.getElements()) {
            auto candidate = std::dynamic_pointer_cast<model::Unit>(element);
            if (candidate && candidate.get() == &unit) {
                return candidate;
            }
        }

        return nullptr;
    }

    void MovementSystem::enqueuePathRequest(
        world::GameWorld&,
        const std::shared_ptr<model::Unit>& unit,
        const model::Vector2D& target,
        const PathOrderKind kind,
        std::optional<model::Vector2D> patrolStart) {
        if (!unit || unit->getAction() == model::ActionType::Dead) {
            return;
        }

        const auto id = m_nextPathRequestId++;
        m_latestPathRequestByUnit[unit.get()] = id;

        // Cancel the current motion immediately; A* work itself is spread across ticks.
        if (kind != PathOrderKind::Patrol || patrolStart.has_value()) {
            unit->stop();
        }

        m_pathRequests.push_back(PathRequest {
            .id = id,
            .unit = unit,
            .target = target,
            .kind = kind,
            .patrolStart = patrolStart
        });
    }

    void MovementSystem::processQueuedPathRequests(world::GameWorld& world) {
        std::size_t processed = 0;
        while (processed < kMaxPathRequestsPerTick && !m_pathRequests.empty()) {
            auto request = m_pathRequests.front();
            m_pathRequests.pop_front();

            auto unit = request.unit.lock();
            if (!unit) {
                continue;
            }
            if (unit->getAction() == model::ActionType::Dead) {
                m_latestPathRequestByUnit.erase(unit.get());
                continue;
            }

            auto latest = m_latestPathRequestByUnit.find(unit.get());
            if (latest == m_latestPathRequestByUnit.end() || latest->second != request.id) {
                continue;
            }

            m_latestPathRequestByUnit.erase(latest);
            issuePathOrder(world, *unit, request.target, request.kind, request.patrolStart);
            ++processed;
        }
    }
}
