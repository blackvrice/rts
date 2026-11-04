#include "game/game/systems/EconomySystem.hpp"

#include <iostream>
#include <memory>
#include <string>

#include "core/data/DataRegistry.hpp"
#include "core/model/Building.hpp"
#include "core/model/ResourceNode.hpp"
#include "core/model/Unit.hpp"
#include "core/world/GameWorld.hpp"
#include "core/world/WorldRuntimeServices.hpp"
#include "game/game/systems/GameplayQueries.hpp"

namespace rts::core::manager {
    void EconomySystem::issueGather(
        world::GameWorld& world,
        const SelectionSystem::SelectedList& selected,
        model::ResourceNode& resource) const {
        if (resource.isDepleted()) {
            return;
        }

        for (const auto& weak : selected) {
            auto element = weak.lock();
            auto worker = std::dynamic_pointer_cast<model::Unit>(element);
            if (!worker ||
                !worker->isWorker() ||
                worker->getAction() == model::ActionType::Dead) {
                continue;
            }

            auto dropOff = queries::findClosestDropOffFor(world, *worker);
            if (!dropOff) {
                worker->clearOrderQueue();
                worker->stop();
                continue;
            }

            worker->clearOrderQueue();
            worker->gather(&resource, dropOff.get());
        }
    }

    void EconomySystem::applyReadyResourceDeliveries(world::GameWorld& world) const {
        for (const auto& element : world.getElements()) {
            auto unit = std::dynamic_pointer_cast<model::Unit>(element);
            if (!unit) {
                continue;
            }

            const auto delivery = unit->takeReadyResourceDelivery();
            if (!delivery || delivery->amount <= 0) {
                continue;
            }

            auto resources = world.playerResources(unit->getTeamId());
            switch (delivery->type) {
                case model::ResourceNode::ResourceType::Gold:
                    resources.gold += delivery->amount;
                    break;
                case model::ResourceNode::ResourceType::Wood:
                    resources.wood += delivery->amount;
                    break;
            }

            world.setPlayerResources(unit->getTeamId(), resources);
            world::emitSound(world, world::SoundCue::ResourceGather,
                             unit->getPosition(), 35.0f);
            world::emitEffect(world, world::EffectType::ResourceGather,
                              unit->getPosition(), 26.0f, 0.32f);
        }
    }

    void EconomySystem::handleGatherRedirects(world::GameWorld& world) const {
        for (const auto& element : world.getElements()) {
            auto worker = std::dynamic_pointer_cast<model::Unit>(element);
            if (!worker || !worker->isWorker() ||
                worker->getAction() == model::ActionType::Dead) {
                continue;
            }

            if (worker->isNeedingDropOffRedirect()) {
                auto newDropOff = queries::findClosestDropOffFor(world, *worker);
                if (newDropOff) {
                    worker->redirectToDropOff(newDropOff.get());
                } else {
                    worker->stop();
                }
            } else if (worker->isNeedingResourceRedirect()) {
                auto newResource = queries::findClosestAvailableResource(
                    world, worker->targetGatherType(), *worker);
                auto newDropOff = queries::findClosestDropOffFor(world, *worker);
                if (newResource && newDropOff) {
                    worker->gather(newResource.get(), newDropOff.get());
                } else {
                    worker->stop();
                }
            }
        }
    }

    void EconomySystem::recomputeSupply(world::GameWorld& world) {
        // Recompute population economy from live state each tick so it self-corrects
        // for deaths, cancels and spawns (TeamId indexes 0=Neutral, 1=Player, 2=Enemy):
        //   capacity = sum of providesSupply over completed buildings
        //   foodUsed = food of live units + food of units still in training queues
        //   army     = count of live combat (non-worker) units
        int capacity[3] = { 0, 0, 0 };
        int used[3]     = { 0, 0, 0 };
        int army[3]     = { 0, 0, 0 };
        const auto& registry = data::DataRegistry::global();

        for (const auto& element : world.getElements()) {
            if (auto building = std::dynamic_pointer_cast<model::Building>(element)) {
                if (building->getAction() == model::ActionType::Dead) continue;
                const int team = building->getTeamId();
                if (team < 0 || team > 2) continue;
                if (building->isComplete()) {
                    capacity[team] += registry.building(building->buildingType()).providesSupply;
                }
                // Units still in production reserve their food before they spawn.
                for (int i = 0; i < building->trainQueueSize(); ++i) {
                    used[team] += registry.unit(building->trainQueueAt(i)).foodCost;
                }
            } else if (auto unit = std::dynamic_pointer_cast<model::Unit>(element)) {
                if (unit->getAction() == model::ActionType::Dead) continue;
                const int team = unit->getTeamId();
                if (team < 0 || team > 2) continue;
                used[team] += registry.unit(unit->unitType()).foodCost;
                if (!unit->isWorker()) ++army[team];
            }
        }

        for (const int teamId : { model::TeamId::Player, model::TeamId::Enemy }) {
            auto resources = world.playerResources(teamId);
            resources.foodCapacity = capacity[teamId];
            resources.foodUsed = used[teamId];
            resources.army = army[teamId];
            world.setPlayerResources(teamId, resources);
            logResourceChange(teamId, resources);
        }
        m_resourceLogReady = true;
    }

    void EconomySystem::logResourceChange(
        const int teamId, const model::PlayerResourceState& current) {
        if (teamId < 0 || teamId > 2) return;
        const auto& prev = m_lastResourceLog[teamId];
        const bool changed = !m_resourceLogReady ||
            prev.gold != current.gold || prev.wood != current.wood ||
            prev.foodUsed != current.foodUsed || prev.foodCapacity != current.foodCapacity ||
            prev.army != current.army;
        if (!changed) return;

        if (m_resourceLogReady) {
            const auto delta = [](const int now, const int before) {
                const int d = now - before;
                return (d >= 0 ? "+" : "") + std::to_string(d);
            };
            std::cerr << "[Resource] team " << teamId
                      << " gold=" << current.gold << "(" << delta(current.gold, prev.gold) << ")"
                      << " wood=" << current.wood << "(" << delta(current.wood, prev.wood) << ")"
                      << " food=" << current.foodUsed << "/" << current.foodCapacity
                      << " army=" << current.army << "(" << delta(current.army, prev.army) << ")\n";
        }
        m_lastResourceLog[teamId] = current;
    }
}
