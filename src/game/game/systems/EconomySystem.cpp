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
        // for deaths, cancels and spawns, per player slot:
        //   capacity = sum of providesSupply over completed buildings
        //   foodUsed = food of live units + food of units still in training queues
        //   army     = count of live combat (non-worker) units
        struct Tally {
            int capacity { 0 };
            int used { 0 };
            int army { 0 };
        };
        std::map<int, Tally> tally;
        for (const auto& slot : world.players()) {
            tally[slot.id] = Tally {};
        }
        const auto& registry = data::DataRegistry::global();

        for (const auto& element : world.getElements()) {
            if (auto building = std::dynamic_pointer_cast<model::Building>(element)) {
                if (building->getAction() == model::ActionType::Dead) continue;
                const auto it = tally.find(building->getTeamId());
                if (it == tally.end()) continue;
                if (building->isComplete()) {
                    it->second.capacity += registry.building(building->buildingType()).providesSupply;
                }
                // Units still in production reserve their food before they spawn.
                for (int i = 0; i < building->trainQueueSize(); ++i) {
                    it->second.used += registry.unit(building->trainQueueAt(i)).foodCost;
                }
            } else if (auto unit = std::dynamic_pointer_cast<model::Unit>(element)) {
                if (unit->getAction() == model::ActionType::Dead) continue;
                const auto it = tally.find(unit->getTeamId());
                if (it == tally.end()) continue;
                it->second.used += registry.unit(unit->unitType()).foodCost;
                if (!unit->isWorker()) ++it->second.army;
            }
        }

        for (const auto& [playerId, counts] : tally) {
            auto resources = world.playerResources(playerId);
            resources.foodCapacity = counts.capacity;
            resources.foodUsed = counts.used;
            resources.army = counts.army;
            world.setPlayerResources(playerId, resources);
            logResourceChange(playerId, resources);
        }
        m_resourceLogReady = true;
    }

    void EconomySystem::logResourceChange(
        const int teamId, const model::PlayerResourceState& current) {
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
