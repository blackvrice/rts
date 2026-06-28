#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "core/model/Vector2D.hpp"
#include "core/thread/ThreadPool.hpp"
#include "game/game/systems/SelectionSystem.hpp"

namespace rts::core::model {
    class Unit;
}

namespace rts::core::world {
    class GameWorld;
}

namespace rts::core::manager {
    class CollisionSystem;

    enum class PathOrderKind {
        Move,
        AttackMove,
        Patrol
    };

    class MovementSystem {
    public:
        MovementSystem();

        struct FormationTarget {
            std::shared_ptr<model::Unit> unit;
            model::Vector2D target {};
        };

        void update(world::GameWorld& world, float dt, const CollisionSystem& collision);
        [[nodiscard]] std::vector<FormationTarget> formationTargets(
            world::GameWorld& world,
            const SelectionSystem::SelectedList& selected,
            const model::Vector2D& target) const;
        void issueMove(world::GameWorld& world,
                       const SelectionSystem::SelectedList& selected,
                       const model::Vector2D& target,
                       bool clearQueuedOrders = true);
        void issueMove(world::GameWorld& world,
                       model::Unit& unit,
                       const model::Vector2D& target,
                       bool clearQueuedOrders = true);
        void issueAttackMove(world::GameWorld& world,
                             const SelectionSystem::SelectedList& selected,
                             const model::Vector2D& target);
        void issueAttackMove(world::GameWorld& world,
                             model::Unit& unit,
                             const model::Vector2D& target);
        void issuePatrol(world::GameWorld& world,
                         const SelectionSystem::SelectedList& selected,
                         const model::Vector2D& target);
        void issuePatrol(world::GameWorld& world,
                         model::Unit& unit,
                         const model::Vector2D& target);
        void cancelQueuedPath(model::Unit& unit);
        void cancelQueuedPaths(const SelectionSystem::SelectedList& selected);
        // Drops all queued path state (for a match restart, when every unit is gone).
        void reset();

    private:
        struct PathRequest {
            std::uint64_t id { 0 };
            std::weak_ptr<model::Unit> unit;
            model::Vector2D target {};
            PathOrderKind kind { PathOrderKind::Move };
            std::optional<model::Vector2D> patrolStart {};
            // Periodic replan rather than a fresh order: the unit keeps following its
            // current path until the new one is ready, and a failed replan is ignored.
            bool refresh { false };
        };

        // Per-tick A* budgets are fixed constants (independent of core count) so the
        // simulation is deterministic across machines; the thread pool only makes the
        // fixed batch resolve faster. Real orders get their budget first, replans fill
        // the rest. Replans run in their own queue so they never delay real orders.
        static constexpr std::size_t kMaxPathRequestsPerTick { 6 };
        static constexpr float kRepathInterval { 1.0f };
        static constexpr std::size_t kMaxRepathRequestsPerTick { 6 };

        std::shared_ptr<model::Unit> findUnitHandle(world::GameWorld& world, model::Unit& unit) const;
        void enqueuePathRequest(world::GameWorld& world,
                                const std::shared_ptr<model::Unit>& unit,
                                const model::Vector2D& target,
                                PathOrderKind kind,
                                std::optional<model::Vector2D> patrolStart = std::nullopt);
        // Enqueues a replan for every unit currently traveling a path (gated so a unit
        // with a pending order/replan is left alone).
        void enqueuePeriodicRepaths(world::GameWorld& world);
        // Resolves the queued path requests (real orders + replans) for this tick in
        // parallel on the thread pool, then applies the results in a fixed order.
        void processPathBatch(world::GameWorld& world);

        std::deque<PathRequest> m_pathRequests;
        std::deque<PathRequest> m_repathRequests;
        std::unordered_map<const model::Unit*, std::uint64_t> m_latestPathRequestByUnit;
        std::uint64_t m_nextPathRequestId { 1 };
        float m_repathTimer { 0.0f };
        core::thread::ThreadPool m_pathPool;
    };
}
