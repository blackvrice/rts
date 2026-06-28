#include "game/game/systems/MovementSystem.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <iterator>
#include <latch>
#include <limits>
#include <optional>
#include <thread>
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

    std::size_t unitWorldOrdinal(const GameWorld& world, const Unit& unit) {
        std::size_t ordinal = 0;
        for (const auto& element : world.getElements()) {
            if (auto candidate = std::dynamic_pointer_cast<Unit>(element)) {
                if (candidate.get() == &unit) {
                    return ordinal;
                }
            }
            ++ordinal;
        }

        return ordinal;
    }

    bool sameFormationCell(
        const rts::core::path::GridPos& a,
        const rts::core::path::GridPos& b) {
        return a.x == b.x && a.y == b.y;
    }

    bool cellReserved(
        const std::vector<rts::core::path::GridPos>& reserved,
        const rts::core::path::GridPos& cell) {
        return std::any_of(
            reserved.begin(),
            reserved.end(),
            [&cell](const rts::core::path::GridPos& reservedCell) {
                return sameFormationCell(reservedCell, cell);
            }
        );
    }

    bool isFormationUnit(
        const std::vector<std::shared_ptr<Unit>>& units,
        const rts::core::model::IGameElement& element) {
        return std::any_of(
            units.begin(),
            units.end(),
            [&element](const std::shared_ptr<Unit>& unit) {
                return unit && unit.get() == &element;
            }
        );
    }

    bool occupiedByNonFormationElement(
        const GameWorld& world,
        const std::vector<std::shared_ptr<Unit>>& units,
        const rts::core::path::GridPos& cell) {
        const auto& transform = world.gridTransform();
        for (const auto& element : world.getElements()) {
            const auto gameElement = std::dynamic_pointer_cast<rts::core::model::IGameElement>(element);
            if (!gameElement ||
                gameElement->getAction() == rts::core::model::ActionType::Dead ||
                isFormationUnit(units, *gameElement)) {
                continue;
            }

            if (sameFormationCell(transform.worldToGrid(gameElement->getPosition()), cell)) {
                return true;
            }
        }

        return false;
    }

    bool formationCellAvailable(
        const GameWorld& world,
        const std::vector<std::shared_ptr<Unit>>& units,
        const std::vector<rts::core::path::GridPos>& reserved,
        const rts::core::path::GridPos& cell) {
        const auto& grid = world.gridQuery();
        return grid.inBounds(cell) &&
               !grid.isBlockedStatic(cell) &&
               !cellReserved(reserved, cell) &&
               !occupiedByNonFormationElement(world, units, cell);
    }

    std::optional<Vector2D> findNearestFormationTarget(
        const GameWorld& world,
        const std::vector<std::shared_ptr<Unit>>& units,
        const std::vector<rts::core::path::GridPos>& reserved,
        const Vector2D& desiredTarget) {
        const auto& transform = world.gridTransform();
        const auto desiredCell = transform.worldToGrid(desiredTarget);

        std::optional<rts::core::path::GridPos> bestCell;
        float bestScore = std::numeric_limits<float>::max();
        constexpr int kMaxFormationSlotSearchRadius = 5;

        for (int radius = 0; radius <= kMaxFormationSlotSearchRadius; ++radius) {
            for (int dy = -radius; dy <= radius; ++dy) {
                for (int dx = -radius; dx <= radius; ++dx) {
                    if (std::max(std::abs(dx), std::abs(dy)) != radius) {
                        continue;
                    }

                    const rts::core::path::GridPos candidate {
                        desiredCell.x + dx,
                        desiredCell.y + dy
                    };
                    if (!formationCellAvailable(world, units, reserved, candidate)) {
                        continue;
                    }

                    const Vector2D candidateWorld = transform.gridToWorldCenter(candidate);
                    const float score =
                        distanceSq(candidateWorld, desiredTarget) +
                        static_cast<float>(radius) * transform.tileSize;
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

        if (!bestCell) {
            return std::nullopt;
        }

        return transform.gridToWorldCenter(*bestCell);
    }

    std::vector<std::shared_ptr<Unit>> sortedFormationUnits(
        const GameWorld& world,
        const rts::core::manager::SelectionSystem::SelectedList& selected) {
        std::vector<std::shared_ptr<Unit>> units;
        units.reserve(selected.size());

        for (const auto& weak : selected) {
            auto unit = std::dynamic_pointer_cast<Unit>(weak.lock());
            if (unit && unit->getAction() != rts::core::model::ActionType::Dead) {
                units.push_back(std::move(unit));
            }
        }

        std::stable_sort(
            units.begin(),
            units.end(),
            [&world](const std::shared_ptr<Unit>& lhs, const std::shared_ptr<Unit>& rhs) {
                const auto left = lhs->getPosition();
                const auto right = rhs->getPosition();
                constexpr float kSortEpsilon = 0.001f;
                if (std::abs(left.y - right.y) > kSortEpsilon) {
                    return left.y < right.y;
                }
                if (std::abs(left.x - right.x) > kSortEpsilon) {
                    return left.x < right.x;
                }
                return unitWorldOrdinal(world, *lhs) < unitWorldOrdinal(world, *rhs);
            }
        );

        return units;
    }

    std::vector<rts::core::manager::MovementSystem::FormationTarget> buildFormationTargets(
        const GameWorld& world,
        const std::vector<std::shared_ptr<Unit>>& units,
        const Vector2D& target) {
        std::vector<rts::core::manager::MovementSystem::FormationTarget> assignments;
        assignments.reserve(units.size());

        if (units.empty()) {
            return assignments;
        }

        if (units.size() == 1) {
            assignments.push_back({units.front(), target});
            return assignments;
        }

        const float spacing = std::max(64.0f, world.gridTransform().tileSize);
        const int columns = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(units.size()))));
        const int rows = static_cast<int>((units.size() + static_cast<std::size_t>(columns) - 1) /
                                          static_cast<std::size_t>(columns));
        std::vector<rts::core::path::GridPos> reservedCells;
        reservedCells.reserve(units.size());

        for (std::size_t index = 0; index < units.size(); ++index) {
            const int row = static_cast<int>(index) / columns;
            const int column = static_cast<int>(index) % columns;
            const float offsetX = (static_cast<float>(column) - (static_cast<float>(columns) - 1.0f) * 0.5f) * spacing;
            const float offsetY = (static_cast<float>(row) - (static_cast<float>(rows) - 1.0f) * 0.5f) * spacing;
            const Vector2D desiredTarget { target.x + offsetX, target.y + offsetY };

            auto assignedTarget = findNearestFormationTarget(world, units, reservedCells, desiredTarget);
            if (!assignedTarget) {
                // If no nearby slot is clean, preserve the requested formation point and
                // let the normal path failure/approach logic decide how to recover.
                assignedTarget = desiredTarget;
            } else {
                reservedCells.push_back(world.gridTransform().worldToGrid(*assignedTarget));
            }

            assignments.push_back({ units[index], *assignedTarget });
        }

        return assignments;
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

        // The goal tile of an attack/move onto a structure is itself blocked, so we
        // ring-search outward for the nearest free, reachable approach cell. The
        // radius must clear the largest footprint inflated by the pathing agent
        // radius plus the extra structure clearance padding, so units keep a small
        // visual gap instead of hugging building edges.
        //
        // Each candidate requires a full A* on a large grid, so we must not test
        // every free cell in a ring (a 4x4 footprint exposes ~16 free cells per
        // ring, and the AI can wave a dozen units at one town hall — that was
        // hundreds of cross-map searches per tick, stalling the sim). Instead try
        // the free candidates closest to the requested target first and return the
        // first one that is actually reachable.
        constexpr int kMaxApproachRadius = 8;
        std::vector<rts::core::path::GridPos> candidates;
        for (int radius = 1; radius <= kMaxApproachRadius; ++radius) {
            candidates.clear();
            for (int dy = -radius; dy <= radius; ++dy) {
                for (int dx = -radius; dx <= radius; ++dx) {
                    if (std::max(std::abs(dx), std::abs(dy)) != radius) {
                        continue;
                    }
                    const rts::core::path::GridPos candidate { goal.x + dx, goal.y + dy };
                    if (!world.gridQuery().inBounds(candidate) ||
                        world.gridQuery().isBlockedStatic(candidate) ||
                        world.gridQuery().isBlockedDynamic(candidate)) {
                        continue;
                    }
                    candidates.push_back(candidate);
                }
            }
            std::sort(candidates.begin(), candidates.end(),
                      [&](const auto& a, const auto& b) {
                          return distanceSq(transform.gridToWorldCenter(a), requestedTarget) <
                                 distanceSq(transform.gridToWorldCenter(b), requestedTarget);
                      });

            for (const auto& candidate : candidates) {
                auto path = world.path().findPath(
                    world.gridQuery(), world.collisionVersion(), start, candidate, options);
                if (path && !path->empty()) {
                    return MovePlan{*path, transform.gridToWorldCenter(candidate)};
                }
            }
        }

        return std::nullopt;
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

    bool applyUnitSeparation(
        const GameWorld& world,
        Unit& unit,
        const CollisionSystem& collision) {
        bool moved = false;

        // First evict the unit from any building/resource footprint it overlaps. The
        // push points outward and only reduces penetration, so it is applied directly
        // (not gated) — a unit deep inside a structure escapes over several ticks
        // instead of being permanently stuck.
        if (const auto escape = collision.structureEscapePush(world, unit, unit.getPosition())) {
            unit.setPosition(unit.getPosition() + *escape);
            moved = true;
        }

        const auto push = collision.localAvoidancePush(world, unit, unit.getPosition());
        if (push) {
            const auto pushedPosition = unit.getPosition() + *push;
            // Unit-unit overlaps resolve over several ticks; requiring one push to
            // fully clear the collision would keep fully stacked units permanently stuck.
            if (collision.canApplyUnitSeparationTo(world, unit, pushedPosition)) {
                unit.setPosition(pushedPosition);
                moved = true;
            }
        }

        return moved;
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

    // Outcome of resolving a path request: either a usable path (direct or via an
    // approach cell) to finalTarget, or unresolved.
    struct PathPlan {
        bool resolved { false };
        rts::core::path::Path path;
        Vector2D finalTarget {};
    };

    // Pure path resolution: reads only the world grid (and the thread-safe path
    // cache) and never touches unit state, so a batch of these can run on worker
    // threads in parallel. The result is applied later, single-threaded, in a fixed
    // order — keeping the simulation deterministic regardless of worker interleaving.
    PathPlan resolvePathPlan(
        GameWorld& world,
        const Vector2D& unitPos,
        const Vector2D& target,
        const rts::core::path::PathOptions& options) {
        const auto& transform = world.gridTransform();
        const auto start = transform.worldToGrid(unitPos);
        const auto goal = transform.worldToGrid(target);

        const auto path = world.path().findPath(
            world.gridQuery(), world.collisionVersion(), start, goal, options);
        if (path && !path->empty()) {
            return PathPlan{ true, *path, target };
        }

        const bool canTryApproach =
            world.gridQuery().inBounds(start) && world.gridQuery().inBounds(goal);
        if (canTryApproach) {
            if (auto approach = findApproachPath(world, start, goal, target, options)) {
                // Occupied goals are approached through a nearby free cell instead of retrying forever.
                return PathPlan{ true, approach->path, approach->finalTarget };
            }
        }

        return PathPlan{ false, {}, target };
    }

    // Applies a resolved plan to the unit. Mutates unit state, so it must run
    // single-threaded (after the parallel resolve phase).
    void applyPathPlan(
        GameWorld& world,
        Unit& unit,
        const Vector2D& target,
        PathOrderKind kind,
        std::optional<Vector2D> patrolStart,
        bool refresh,
        const PathPlan& plan) {
        const Vector2D from = patrolStart.value_or(unit.getPosition());

        // A fresh order halts current motion before re-pathing; a periodic replan
        // lets the unit keep gliding on its existing path until the new one is set,
        // so there is no per-interval stutter.
        if (!refresh && (kind != PathOrderKind::Patrol || patrolStart.has_value())) {
            unit.stop();
        }

        if (plan.resolved) {
            switch (kind) {
                case PathOrderKind::Move:
                    unit.setMoveTargetWithPath(plan.path, plan.finalTarget);
                    break;
                case PathOrderKind::AttackMove:
                    unit.setAttackMoveTargetWithPath(plan.path, plan.finalTarget);
                    break;
                case PathOrderKind::Patrol:
                    if (patrolStart.has_value()) {
                        unit.setPatrolRouteWithPath(plan.path, plan.finalTarget, from, target);
                    } else {
                        unit.setPatrolTargetWithPath(plan.path, plan.finalTarget);
                    }
                    break;
            }
            return;
        }

        // A failed periodic replan is silent: the unit keeps its current path.
        if (!refresh) {
            const auto& transform = world.gridTransform();
            const auto start = transform.worldToGrid(unit.getPosition());
            const auto goal = transform.worldToGrid(target);
            logPathFailure(kind, start, goal, target, pathFailureReason(world, start, goal));
            unit.stop();
        }
    }

    bool issuePathOrder(
        GameWorld& world,
        Unit& unit,
        const Vector2D& target,
        PathOrderKind kind,
        std::optional<Vector2D> patrolStart = std::nullopt,
        bool refresh = false) {
        const auto options = movementPathOptions();
        const PathPlan plan = resolvePathPlan(world, unit.getPosition(), target, options);
        applyPathPlan(world, unit, target, kind, patrolStart, refresh, plan);
        return plan.resolved;
    }

    // Resolved A* request bound to a unit; the unitPos snapshot lets the parallel
    // resolve avoid touching the unit at all.
    struct PathJob {
        std::weak_ptr<Unit> unit;
        Vector2D unitPos {};
        Vector2D target {};
        PathOrderKind kind { PathOrderKind::Move };
        std::optional<Vector2D> patrolStart {};
        bool refresh { false };
        PathPlan plan {};
    };

    std::size_t pathPoolWorkerCount() {
        const unsigned hardware = std::thread::hardware_concurrency();
        // Reserve a core for the logic thread (and render/audio); fall back to
        // single-threaded resolution on low-core machines.
        if (hardware <= 2) {
            return 0;
        }
        return static_cast<std::size_t>(hardware - 1);
    }
}

namespace rts::core::manager {
    MovementSystem::MovementSystem()
        : m_pathPool(pathPoolWorkerCount()) {
    }

    void MovementSystem::update(
        world::GameWorld& world,
        float dt,
        const CollisionSystem& collision) {
        // Periodically re-plan traveling units so paths track the changing world
        // (new/destroyed buildings, congestion). Replans are queued separately and
        // only fill the path budget left over by real orders.
        m_repathTimer += dt;
        if (m_repathTimer >= kRepathInterval) {
            m_repathTimer = 0.0f;
            enqueuePeriodicRepaths(world);
        }

        // Resolve this tick's queued path requests (real orders first, then replans)
        // in parallel on the thread pool, then apply them in a fixed order.
        processPathBatch(world);

        bool worldCollisionChanged = false;
        for (const auto& element : world.getElements()) {
            if (auto unit = std::dynamic_pointer_cast<model::Unit>(element)) {
                const auto previousPosition = unit->getPosition();
                bool separatedFromUnits = false;
                if (unit->getAction() == model::ActionType::Move ||
                    unit->getAction() == model::ActionType::Patrol) {
                    unit->updateMove(dt, world.gridTransform());
                } else {
                    unit->tick(dt);
                }

                if (unit->getAction() != model::ActionType::Dead) {
                    separatedFromUnits = applyUnitSeparation(world, *unit, collision);
                }

                // Movement is speculative until the collision system accepts the next
                // position. Gather/Build workers deliberately approach a structure's
                // footprint edge (bounded by their interact range), so they are exempt
                // from the revert that would otherwise stop them short of the building.
                const auto act = unit->getAction();
                if (act != model::ActionType::Dead &&
                    act != model::ActionType::Gather &&
                    act != model::ActionType::Build) {
                    const auto hit = collision.findMoveBlocker(world, *unit, unit->getPosition());
                    if (hit.has_value() && !(separatedFromUnits && hit->isUnit)) {
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
                    worldCollisionChanged = true;
                }
                continue;
            }

            if (auto gameElement = std::dynamic_pointer_cast<model::IGameElement>(element)) {
                gameElement->tick(dt);
            }
        }

        if (worldCollisionChanged) {
            world.onCollisionChanged();
        }
    }

    std::vector<MovementSystem::FormationTarget> MovementSystem::formationTargets(
        world::GameWorld& world,
        const SelectionSystem::SelectedList& selected,
        const model::Vector2D& target) const {
        return buildFormationTargets(world, sortedFormationUnits(world, selected), target);
    }

    void MovementSystem::issueMove(
        world::GameWorld& world,
        const SelectionSystem::SelectedList& selected,
        const model::Vector2D& target,
        bool clearQueuedOrders) {
        const auto assignments = formationTargets(world, selected, target);
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
            }
        }

        for (const auto& assignment : assignments) {
            if (!assignment.unit || assignment.unit->getAction() == model::ActionType::Dead) {
                continue;
            }

            if (clearQueuedOrders) {
                assignment.unit->clearOrderQueue();
            }
            enqueuePathRequest(world, assignment.unit, assignment.target, PathOrderKind::Move);
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
        for (const auto& assignment : formationTargets(world, selected, target)) {
            if (assignment.unit && assignment.unit->getAction() != model::ActionType::Dead) {
                enqueuePathRequest(world, assignment.unit, assignment.target, PathOrderKind::AttackMove);
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
        for (const auto& assignment : formationTargets(world, selected, target)) {
            if (assignment.unit && assignment.unit->getAction() != model::ActionType::Dead) {
                enqueuePathRequest(world, assignment.unit, assignment.target, PathOrderKind::Patrol, assignment.unit->getPosition());
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

    void MovementSystem::reset() {
        m_pathRequests.clear();
        m_repathRequests.clear();
        m_latestPathRequestByUnit.clear();
        m_nextPathRequestId = 1;
        m_repathTimer = 0.0f;
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

    void MovementSystem::processPathBatch(world::GameWorld& world) {
        std::vector<PathJob> jobs;

        // Drains up to `maxCount` live requests from a queue into `jobs`, snapshotting
        // each unit's position (the resolve phase must not touch units). Mirrors the
        // old per-queue gating: skip dead/superseded units, and for replans skip any
        // unit no longer traveling.
        const auto collect = [&](std::deque<PathRequest>& queue,
                                 std::size_t maxCount,
                                 bool requireTraveling) {
            std::size_t taken = 0;
            while (taken < maxCount && !queue.empty()) {
                auto request = queue.front();
                queue.pop_front();

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
                    continue;  // superseded by a newer request
                }
                m_latestPathRequestByUnit.erase(latest);

                if (requireTraveling) {
                    const auto action = unit->getAction();
                    if (action != model::ActionType::Move &&
                        action != model::ActionType::Patrol) {
                        continue;
                    }
                }

                jobs.push_back(PathJob {
                    .unit = unit,
                    .unitPos = unit->getPosition(),
                    .target = request.target,
                    .kind = request.kind,
                    .patrolStart = request.patrolStart,
                    .refresh = request.refresh,
                    .plan = {}
                });
                ++taken;
            }
        };

        // Real orders claim their budget first; replans fill what is left.
        collect(m_pathRequests, kMaxPathRequestsPerTick, false);
        collect(m_repathRequests, kMaxRepathRequestsPerTick, true);
        if (jobs.empty()) {
            return;
        }

        const auto options = movementPathOptions();

        // Resolve every request in parallel (pure grid reads; no unit mutation), then
        // apply results in `jobs` order — so the outcome is independent of how the
        // worker threads interleave and the simulation stays deterministic.
        if (jobs.size() > 1 && m_pathPool.workerCount() > 0) {
            std::latch done(static_cast<std::ptrdiff_t>(jobs.size()));
            for (std::size_t i = 0; i < jobs.size(); ++i) {
                m_pathPool.submit([&jobs, i, &world, &options, &done] {
                    jobs[i].plan = resolvePathPlan(world, jobs[i].unitPos, jobs[i].target, options);
                    done.count_down();
                });
            }
            done.wait();
        } else {
            for (auto& job : jobs) {
                job.plan = resolvePathPlan(world, job.unitPos, job.target, options);
            }
        }

        for (auto& job : jobs) {
            auto unit = job.unit.lock();
            if (!unit) {
                continue;
            }
            applyPathPlan(world, *unit, job.target, job.kind, job.patrolStart, job.refresh, job.plan);
        }
    }

    void MovementSystem::enqueuePeriodicRepaths(world::GameWorld& world) {
        for (const auto& element : world.getElements()) {
            auto unit = std::dynamic_pointer_cast<model::Unit>(element);
            if (!unit) {
                continue;
            }

            // Only re-plan units that are actually traveling a path. Gather/Build/
            // Attack/Hold/Idle units are steered by their own logic, not a path tail.
            const auto action = unit->getAction();
            if (action != model::ActionType::Move &&
                action != model::ActionType::Patrol) {
                continue;
            }

            // Leave any unit that already has a pending order or replan alone, so a
            // replan never clobbers a freshly issued command.
            if (m_latestPathRequestByUnit.contains(unit.get())) {
                continue;
            }

            PathOrderKind kind = PathOrderKind::Move;
            if (unit->isPatrolActive()) {
                kind = PathOrderKind::Patrol;
            } else if (unit->isAttackMoveActive()) {
                kind = PathOrderKind::AttackMove;
            }

            const auto id = m_nextPathRequestId++;
            m_latestPathRequestByUnit[unit.get()] = id;
            m_repathRequests.push_back(PathRequest {
                .id = id,
                .unit = unit,
                .target = unit->finalTargetWorld(),
                .kind = kind,
                .patrolStart = std::nullopt,
                .refresh = true
            });
        }
    }

}
