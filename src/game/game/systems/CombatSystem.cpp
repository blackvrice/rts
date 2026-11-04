#include "game/game/systems/CombatSystem.hpp"

#include <algorithm>
#include <limits>

#include "core/model/IGameElement.hpp"
#include "core/model/Unit.hpp"
#include "core/world/GameWorld.hpp"
#include "core/world/WorldRuntimeServices.hpp"
#include "game/game/systems/GameplayQueries.hpp"
#include "game/game/systems/MovementSystem.hpp"

namespace rts::core::manager {
    namespace {
        constexpr float kAttackMoveAcquireRadius = 220.0f;
        // Upper bound on a target's body radius (largest building footprint half-
        // diagonal) used to widen acquisition queries before per-target edge gating.
        constexpr float kMaxTargetHitRadius = 200.0f;
    }

    void CombatSystem::update(
        world::GameWorld& world, MovementSystem& movement, const bool enemyAiPassive) const {
        handleAttackRetargets(world);
        handleAttackMoveOrders(world, movement);
        handlePatrolOrders(world, movement);
        handleHoldPositionOrders(world);
        handleIdleAutoAcquire(world, enemyAiPassive);
    }

    std::shared_ptr<model::IGameElement> CombatSystem::findClosestAcquireTarget(
        const world::GameWorld& world, const model::Unit& unit) {
        std::shared_ptr<model::IGameElement> bestTarget;
        float bestDistanceSq = std::numeric_limits<float>::max();
        const float acquireRadius = std::max(kAttackMoveAcquireRadius, unit.getAttackRange());
        const float acquireRadiusSq = acquireRadius * acquireRadius;

        for (auto candidate : world::queryRadius(world, unit.getPosition(), acquireRadius)) {
            if (!candidate || !unit.canAttackTarget(candidate.get())) {
                continue;
            }

            const float candidateDistanceSq = queries::distanceSq(
                candidate->getPosition(),
                unit.getPosition()
            );
            if (candidateDistanceSq <= acquireRadiusSq &&
                candidateDistanceSq < bestDistanceSq) {
                bestDistanceSq = candidateDistanceSq;
                bestTarget = candidate;
            }
        }

        return bestTarget;
    }

    void CombatSystem::handleAttackMoveOrders(world::GameWorld& world, MovementSystem& movement) {
        for (const auto& element : world.getElements()) {
            auto unit = std::dynamic_pointer_cast<model::Unit>(element);
            if (!unit ||
                unit->getAction() == model::ActionType::Dead ||
                !unit->isAttackMoveActive()) {
                continue;
            }

            if (unit->isAttackMoveSearching()) {
                if (auto target = findClosestAcquireTarget(world, *unit)) {
                    unit->attackMoveEngage(target.get());
                    continue;
                }
            }

            if (unit->needsAttackMoveResume()) {
                movement.issueAttackMove(world, *unit, unit->attackMoveTarget());
            } else if (unit->getAction() == model::ActionType::Idle) {
                unit->stop();
            }
        }
    }

    void CombatSystem::handleAttackRetargets(world::GameWorld& world) {
        for (const auto& element : world.getElements()) {
            auto unit = std::dynamic_pointer_cast<model::Unit>(element);
            if (!unit ||
                unit->getAction() == model::ActionType::Dead ||
                !unit->needsAttackRetarget()) {
                continue;
            }

            // A queued command takes precedence over auto-retargeting: once the
            // direct attack target dies, fall through to the next queued order.
            if (unit->hasQueuedOrders()) {
                unit->clearAttackRetarget();
                continue;
            }

            if (auto target = findClosestAcquireTarget(world, *unit)) {
                unit->attack(target.get());
            } else {
                unit->clearAttackRetarget();
            }
        }
    }

    void CombatSystem::handlePatrolOrders(world::GameWorld& world, MovementSystem& movement) {
        for (const auto& element : world.getElements()) {
            auto unit = std::dynamic_pointer_cast<model::Unit>(element);
            if (!unit ||
                unit->getAction() == model::ActionType::Dead ||
                !unit->isPatrolActive()) {
                continue;
            }

            if (unit->isPatrolSearching()) {
                if (auto target = findClosestAcquireTarget(world, *unit)) {
                    unit->patrolEngage(target.get());
                    continue;
                }
            }

            if (unit->needsPatrolResume()) {
                movement.issuePatrol(world, *unit, unit->patrolDestination());
            }
        }
    }

    std::shared_ptr<model::IGameElement> CombatSystem::findClosestHoldTarget(
        const world::GameWorld& world, const model::Unit& unit) {
        std::shared_ptr<model::IGameElement> bestTarget;
        float bestDistanceSq = std::numeric_limits<float>::max();
        // Range is gated per-candidate (weapon range + that target's body radius),
        // so the query is widened by the largest footprint reach to still surface a
        // big building whose center sits just past weapon range.
        const float queryRadius = unit.getAttackRange() + kMaxTargetHitRadius;

        for (auto candidate : world::queryRadius(world, unit.getPosition(), queryRadius)) {
            if (!candidate || !unit.canAttackTarget(candidate.get())) {
                continue;
            }

            const float effectiveRange = unit.getAttackRange() + candidate->attackHitRadius();
            const float candidateDistanceSq = queries::distanceSq(
                candidate->getPosition(),
                unit.getPosition()
            );
            if (candidateDistanceSq <= effectiveRange * effectiveRange &&
                candidateDistanceSq < bestDistanceSq) {
                bestDistanceSq = candidateDistanceSq;
                bestTarget = candidate;
            }
        }

        return bestTarget;
    }

    void CombatSystem::handleHoldPositionOrders(world::GameWorld& world) {
        for (const auto& element : world.getElements()) {
            auto unit = std::dynamic_pointer_cast<model::Unit>(element);
            if (!unit ||
                unit->getAction() == model::ActionType::Dead ||
                !unit->isHoldingPosition()) {
                continue;
            }

            if (auto target = findClosestHoldTarget(world, *unit)) {
                unit->holdEngage(target.get());
            }
        }
    }

    std::shared_ptr<model::IGameElement> CombatSystem::findClosestIdleTarget(
        const world::GameWorld& world, const model::Unit& unit) {
        std::shared_ptr<model::IGameElement> bestTarget;
        float bestDistanceSq = std::numeric_limits<float>::max();
        // An idle unit watches its full sight radius for enemies to engage, with
        // the shared acquire radius and its own weapon range as lower bounds so a
        // long-range or wide-sighted unit still reaches what it can perceive.
        const float sightWorld = unit.getSightRange() * world.gridTransform().tileSize;
        const float acquireRadius = std::max(
            { kAttackMoveAcquireRadius, unit.getAttackRange(), sightWorld });
        const float acquireRadiusSq = acquireRadius * acquireRadius;

        for (auto candidate : world::queryRadius(world, unit.getPosition(), acquireRadius)) {
            if (!candidate || !unit.canAttackTarget(candidate.get())) {
                continue;
            }

            const float candidateDistanceSq = queries::distanceSq(
                candidate->getPosition(),
                unit.getPosition()
            );
            if (candidateDistanceSq <= acquireRadiusSq &&
                candidateDistanceSq < bestDistanceSq) {
                bestDistanceSq = candidateDistanceSq;
                bestTarget = candidate;
            }
        }

        return bestTarget;
    }

    void CombatSystem::handleIdleAutoAcquire(world::GameWorld& world, const bool enemyAiPassive) {
        for (const auto& element : world.getElements()) {
            auto unit = std::dynamic_pointer_cast<model::Unit>(element);
            // Only genuinely idle, combat-capable units acquire on their own.
            // Move/Attack/Hold/Gather/Build all report a non-Idle action, so a unit
            // carrying out any explicit order is skipped here. Workers keep mining,
            // and units already steered by another pass (attack-move, patrol,
            // post-kill retarget, or a queued order) are left to that pass.
            if (!unit || unit->getAction() != model::ActionType::Idle) {
                continue;
            }
            if (unit->isWorker() ||
                unit->isAttackMoveActive() ||
                unit->isPatrolActive() ||
                unit->needsAttackRetarget() ||
                unit->hasQueuedOrders()) {
                continue;
            }
            // Passive mode (MatchConfig debug switch): the enemy never engages on
            // its own; only the player's idle units auto-acquire nearby threats.
            if (enemyAiPassive && unit->getTeamId() == model::TeamId::Enemy) {
                continue;
            }

            if (auto target = findClosestIdleTarget(world, *unit)) {
                unit->attack(target.get());
            }
        }
    }
}
