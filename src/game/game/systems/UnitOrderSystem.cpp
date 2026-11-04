#include "game/game/systems/UnitOrderSystem.hpp"

#include <memory>

#include "core/command/LogicCommand.hpp"
#include "core/ecs/EntityId.hpp"
#include "core/model/Building.hpp"
#include "core/model/ResourceNode.hpp"
#include "core/model/Unit.hpp"
#include "core/world/GameWorld.hpp"
#include "core/world/WorldRuntimeServices.hpp"
#include "game/game/systems/EconomySystem.hpp"
#include "game/game/systems/GameplayQueries.hpp"
#include "game/game/systems/MovementSystem.hpp"

namespace rts::core::manager {
    void UnitOrderSystem::move(
        world::GameWorld& world, const SelectionSystem::SelectedList& selected,
        const command::MoveCommand& cmd) {
        if (cmd.append()) {
            queueMoveOrderForSelected(world, selected, cmd.target());
            return;
        }

        // A move order on a selected production building sets its rally point instead
        // of moving the (immobile) building.
        for (const auto& weak : selected) {
            if (auto building = std::dynamic_pointer_cast<model::Building>(weak.lock());
                building && building->getAction() != model::ActionType::Dead) {
                building->setRallyPoint(cmd.target());
                world::emitEffect(world, world::EffectType::ConstructionDust,
                                  cmd.target(), 34.0f, 0.35f);
            }
        }
        world::emitSound(world, world::SoundCue::MoveOrder, cmd.target(), 48.0f);
        m_movement.issueMove(world, selected, cmd.target());
    }

    void UnitOrderSystem::attack(
        world::GameWorld& world, const SelectionSystem::SelectedList& selected,
        const command::AttackCommand& cmd) {
        if (!cmd.hasWorldTarget()) {
            return;
        }

        int attackerTeam = model::TeamId::Neutral;
        for (const auto& weak : selected) {
            if (auto unit = weak.lock(); unit && unit->getAction() != model::ActionType::Dead) {
                attackerTeam = unit->getTeamId();
                break;
            }
        }

        // Prefer the EntityId the UI captured under the cursor, but only when it
        // resolves to a live resource or opposing unit near the click — otherwise
        // fall back to selection-aware positional resolution (unchanged UX).
        std::shared_ptr<model::IGameElement> target;
        if (ecs::isValid(cmd.targetEntityId())) {
            if (auto byId = world.resolve(cmd.targetEntityId())) {
                const bool validKind = std::dynamic_pointer_cast<model::ResourceNode>(byId) ||
                                       queries::isOpposingTeam(attackerTeam, byId->getTeamId());
                const bool nearClick =
                    queries::distanceSq(byId->getPosition(), cmd.target()) <= queries::kCommandPickRadiusSq;
                if (validKind && nearClick) {
                    target = std::move(byId);
                }
            }
        }
        if (!target) {
            target = queries::findCommandTargetAt(world, cmd.target(), selected);
        }
        if (!target) {
            if (cmd.append()) {
                queueMoveOrderForSelected(world, selected, cmd.target());
                world::emitSound(world, world::SoundCue::MoveOrder, cmd.target(), 44.0f);
                return;
            }
            world::emitSound(world, world::SoundCue::MoveOrder, cmd.target(), 48.0f);
            m_movement.issueMove(world, selected, cmd.target());
            return;
        }

        // Shift-append: queue the smart action behind any existing orders so the
        // player can chain move/attack/gather steps (the order resolves its target
        // by EntityId when it runs).
        if (cmd.append()) {
            if (std::dynamic_pointer_cast<model::ResourceNode>(target)) {
                enqueueOrderForSelected(
                    world, selected,
                    model::UnitOrder { .type = model::OrderType::Gather,
                                       .targetEntityId = target->entityId() },
                    /*workersOnly=*/true);
                world::emitSound(world, world::SoundCue::ResourceGather,
                                 target->getPosition(), 42.0f);
            } else if (queries::isOpposingTeam(attackerTeam, target->getTeamId())) {
                enqueueOrderForSelected(
                    world, selected,
                    model::UnitOrder { .type = model::OrderType::Attack,
                                       .targetEntityId = target->entityId() },
                    /*workersOnly=*/false);
                world::emitSound(world, world::SoundCue::AttackOrder,
                                 target->getPosition(), 56.0f);
            } else {
                queueMoveOrderForSelected(world, selected, target->getPosition());
                world::emitSound(world, world::SoundCue::MoveOrder,
                                 target->getPosition(), 44.0f);
            }
            return;
        }

        if (auto resource = std::dynamic_pointer_cast<model::ResourceNode>(target)) {
            clearSelectedUnitOrderQueues(selected);
            m_economy.issueGather(world, selected, *resource);
            world::emitSound(world, world::SoundCue::ResourceGather,
                             resource->getPosition(), 46.0f);
            return;
        }

        // Right-click follows RTS convention: friendly target means move/approach,
        // opposing team target means attack.
        if (!queries::isOpposingTeam(attackerTeam, target->getTeamId())) {
            world::emitSound(world, world::SoundCue::MoveOrder,
                             target->getPosition(), 44.0f);
            m_movement.issueMove(world, selected, target->getPosition());
            return;
        }

        clearSelectedUnitOrderQueues(selected);
        world::emitSound(world, world::SoundCue::AttackOrder,
                         target->getPosition(), 60.0f);
        for (const auto& weak : selected) {
            if (auto attacker = weak.lock();
                attacker &&
                attacker.get() != target.get() &&
                attacker->getAction() != model::ActionType::Dead) {
                attacker->attack(target.get());
            }
        }
    }

    void UnitOrderSystem::attackMove(
        world::GameWorld& world, const SelectionSystem::SelectedList& selected,
        const command::AttackMoveCommand& cmd) {
        clearSelectedUnitOrderQueues(selected);
        world::emitSound(world, world::SoundCue::AttackOrder, cmd.target(), 54.0f);
        m_movement.issueAttackMove(world, selected, cmd.target());
    }

    void UnitOrderSystem::patrol(
        world::GameWorld& world, const SelectionSystem::SelectedList& selected,
        const command::PatrolCommand& cmd) {
        clearSelectedUnitOrderQueues(selected);
        world::emitSound(world, world::SoundCue::MoveOrder, cmd.to(), 42.0f);
        m_movement.issuePatrol(world, selected, cmd.to());
    }

    void UnitOrderSystem::gather(
        world::GameWorld& world, const SelectionSystem::SelectedList& selected,
        const command::GatherCommand& cmd) {
        if (!cmd.hasWorldTarget()) {
            return;
        }

        const auto target = queries::findCommandTargetAt(world, cmd.target(), selected);
        auto resource = std::dynamic_pointer_cast<model::ResourceNode>(target);
        if (!resource) {
            return;
        }

        clearSelectedUnitOrderQueues(selected);
        m_economy.issueGather(world, selected, *resource);
        world::emitSound(world, world::SoundCue::ResourceGather,
                         resource->getPosition(), 46.0f);
    }

    void UnitOrderSystem::stop(const SelectionSystem::SelectedList& selected) {
        m_movement.cancelQueuedPaths(selected);
        for (const auto& weak : selected) {
            if (auto element = weak.lock(); element && element->getAction() != model::ActionType::Dead) {
                if (auto unit = std::dynamic_pointer_cast<model::Unit>(element)) {
                    unit->clearOrderQueue();
                }
                element->stop();
            }
        }
    }

    void UnitOrderSystem::hold(const SelectionSystem::SelectedList& selected) {
        m_movement.cancelQueuedPaths(selected);
        for (const auto& weak : selected) {
            if (auto element = weak.lock(); element && element->getAction() != model::ActionType::Dead) {
                if (auto unit = std::dynamic_pointer_cast<model::Unit>(element)) {
                    unit->clearOrderQueue();
                }
                element->holdPosition();
            }
        }
    }

    void UnitOrderSystem::clearSelectedUnitOrderQueues(const SelectionSystem::SelectedList& selected) {
        m_movement.cancelQueuedPaths(selected);
        for (const auto& weak : selected) {
            auto unit = std::dynamic_pointer_cast<model::Unit>(weak.lock());
            if (unit && unit->getAction() != model::ActionType::Dead) {
                unit->clearOrderQueue();
            }
        }
    }

    void UnitOrderSystem::queueMoveOrderForSelected(
        world::GameWorld& world, const SelectionSystem::SelectedList& selected,
        const model::Vector2D& target) {
        for (const auto& assignment : m_movement.formationTargets(world, selected, target)) {
            auto unit = assignment.unit;
            if (!unit || unit->getAction() == model::ActionType::Dead) {
                continue;
            }

            unit->enqueueOrder(model::UnitOrder {
                .type = model::OrderType::Move,
                .targetPosition = assignment.target
            });

            // Shift-clicking while idle should still begin the first queued waypoint.
            if (unit->getAction() == model::ActionType::Idle) {
                issueNextQueuedOrder(world, *unit);
            }
        }
    }

    void UnitOrderSystem::enqueueOrderForSelected(
        world::GameWorld& world, const SelectionSystem::SelectedList& selected,
        const model::UnitOrder& order, const bool workersOnly) {
        for (const auto& weak : selected) {
            auto unit = std::dynamic_pointer_cast<model::Unit>(weak.lock());
            if (!unit || unit->getAction() == model::ActionType::Dead) {
                continue;
            }
            if (workersOnly && !unit->isWorker()) {
                continue;
            }
            unit->enqueueOrder(order);
            // Kick off the first queued step right away when the unit is idle.
            if (unit->getAction() == model::ActionType::Idle) {
                issueNextQueuedOrder(world, *unit);
            }
        }
    }

    void UnitOrderSystem::issueNextQueuedOrder(world::GameWorld& world, model::Unit& unit) {
        // Pop orders until one can be started; dead/invalid targets are skipped so
        // the queue advances instead of stalling.
        while (auto order = unit.popNextOrder()) {
            switch (order->type) {
                case model::OrderType::Move:
                    m_movement.issueMove(world, unit, order->targetPosition, false);
                    return;
                case model::OrderType::AttackMove:
                    m_movement.issueAttackMove(world, unit, order->targetPosition);
                    return;
                case model::OrderType::Patrol:
                    m_movement.issuePatrol(world, unit, order->targetPosition);
                    return;
                case model::OrderType::Attack: {
                    auto target = world.resolve(order->targetEntityId);
                    if (target && target.get() != &unit &&
                        target->getAction() != model::ActionType::Dead) {
                        unit.attack(target.get());
                        return;
                    }
                    break;  // target gone: fall through to the next queued order
                }
                case model::OrderType::Gather: {
                    auto resource = std::dynamic_pointer_cast<model::ResourceNode>(
                        world.resolve(order->targetEntityId));
                    if (unit.isWorker() && resource && !resource->isDepleted()) {
                        if (auto dropOff = queries::findClosestDropOffFor(world, unit)) {
                            unit.gather(resource.get(), dropOff.get());
                            return;
                        }
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }

    void UnitOrderSystem::handleQueuedOrders(world::GameWorld& world) {
        for (const auto& element : world.getElements()) {
            auto unit = std::dynamic_pointer_cast<model::Unit>(element);
            if (!unit ||
                unit->getAction() != model::ActionType::Idle ||
                !unit->hasQueuedOrders()) {
                continue;
            }

            issueNextQueuedOrder(world, *unit);
        }
    }
}
