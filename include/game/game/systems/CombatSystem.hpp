#pragma once

#include <memory>

namespace rts::core::model {
    class IGameElement;
    class Unit;
}

namespace rts::core::world {
    class GameWorld;
}

namespace rts::core::manager {
    class MovementSystem;

    // Automatic target acquisition for units that are not under a direct attack
    // order: post-kill retargeting, attack-move, patrol, hold position and idle
    // auto-acquire. Direct attack orders themselves live in UnitOrderSystem;
    // damage is applied by the units/projectiles.
    class CombatSystem {
    public:
        // Runs the engagement passes in their fixed order (retarget, attack-move,
        // patrol, hold, idle). The order matters for determinism and for which
        // pass claims a unit first. enemyAiPassive stops idle enemy units from
        // engaging on their own (MatchConfig debug switch).
        void update(world::GameWorld& world, MovementSystem& movement, bool enemyAiPassive) const;

    private:
        static void handleAttackRetargets(world::GameWorld& world);
        static void handleAttackMoveOrders(world::GameWorld& world, MovementSystem& movement);
        static void handlePatrolOrders(world::GameWorld& world, MovementSystem& movement);
        static void handleHoldPositionOrders(world::GameWorld& world);
        static void handleIdleAutoAcquire(world::GameWorld& world, bool enemyAiPassive);

        // Closest attackable enemy within max(shared acquire radius, weapon range).
        // Used by attack-move, post-kill retarget and patrol.
        static std::shared_ptr<model::IGameElement> findClosestAcquireTarget(
            const world::GameWorld& world, const model::Unit& unit);
        // Closest attackable enemy already inside weapon range (+ target body radius).
        static std::shared_ptr<model::IGameElement> findClosestHoldTarget(
            const world::GameWorld& world, const model::Unit& unit);
        // Closest attackable enemy within the unit's sight radius.
        static std::shared_ptr<model::IGameElement> findClosestIdleTarget(
            const world::GameWorld& world, const model::Unit& unit);
    };
}
