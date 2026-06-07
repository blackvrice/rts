#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "core/model/Vector2D.hpp"
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

    private:
        struct PathRequest {
            std::uint64_t id { 0 };
            std::weak_ptr<model::Unit> unit;
            model::Vector2D target {};
            PathOrderKind kind { PathOrderKind::Move };
            std::optional<model::Vector2D> patrolStart {};
        };

        static constexpr std::size_t kMaxPathRequestsPerTick { 8 };

        std::shared_ptr<model::Unit> findUnitHandle(world::GameWorld& world, model::Unit& unit) const;
        void enqueuePathRequest(world::GameWorld& world,
                                const std::shared_ptr<model::Unit>& unit,
                                const model::Vector2D& target,
                                PathOrderKind kind,
                                std::optional<model::Vector2D> patrolStart = std::nullopt);
        void processQueuedPathRequests(world::GameWorld& world);

        std::deque<PathRequest> m_pathRequests;
        std::unordered_map<const model::Unit*, std::uint64_t> m_latestPathRequestByUnit;
        std::uint64_t m_nextPathRequestId { 1 };
    };
}
