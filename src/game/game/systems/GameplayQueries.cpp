#include "game/game/systems/GameplayQueries.hpp"

#include <limits>

#include "core/data/BuildingStaticData.hpp"
#include "core/model/Building.hpp"
#include "core/model/IGameElement.hpp"
#include "core/model/Unit.hpp"
#include "core/world/GameWorld.hpp"
#include "core/world/WorldRuntimeServices.hpp"

namespace rts::core::manager::queries {
    bool isOpposingTeam(const int attackerTeamId, const int targetTeamId) {
        return attackerTeamId != model::TeamId::Neutral &&
               targetTeamId != model::TeamId::Neutral &&
               attackerTeamId != targetTeamId;
    }

    bool selectionContains(
        const SelectionSystem::SelectedList& selected,
        const model::IGameElement* candidate) {
        for (const auto& weak : selected) {
            if (auto element = weak.lock(); element.get() == candidate) {
                return true;
            }
        }
        return false;
    }

    std::shared_ptr<model::IGameElement> findCommandTargetAt(
        const world::GameWorld& world,
        const model::Vector2D& target,
        const SelectionSystem::SelectedList& selected) {
        std::shared_ptr<model::IGameElement> bestTarget;
        float bestDistanceSq = std::numeric_limits<float>::max();

        for (auto candidate : world::queryRadius(world, target, kCommandPickRadius)) {
            if (!candidate ||
                candidate->getAction() == model::ActionType::Dead) {
                continue;
            }

            const bool alreadySelected = selectionContains(selected, candidate.get());
            const bool selectedResource =
                alreadySelected && std::dynamic_pointer_cast<model::ResourceNode>(candidate);
            // Drag-select can include resource nodes; workers still need the selected resource as a gather target.
            if (alreadySelected && !selectedResource) {
                continue;
            }

            const float candidateDistanceSq = distanceSq(candidate->getPosition(), target);
            if (candidateDistanceSq <= kCommandPickRadiusSq &&
                candidateDistanceSq < bestDistanceSq) {
                bestDistanceSq = candidateDistanceSq;
                bestTarget = std::move(candidate);
            }
        }

        return bestTarget;
    }

    std::shared_ptr<model::Building> findClosestDropOffFor(
        const world::GameWorld& world, const model::Unit& worker) {
        std::shared_ptr<model::Building> bestDropOff;
        float bestDistanceSq = std::numeric_limits<float>::max();

        for (const auto& element : world.getElements()) {
            auto building = std::dynamic_pointer_cast<model::Building>(element);
            if (!building ||
                !building->isDropOff() ||
                building->getAction() == model::ActionType::Dead ||
                building->getTeamId() != worker.getTeamId()) {
                continue;
            }

            const float candidateDistanceSq = distanceSq(
                building->getPosition(),
                worker.getPosition()
            );
            if (candidateDistanceSq < bestDistanceSq) {
                bestDistanceSq = candidateDistanceSq;
                bestDropOff = std::move(building);
            }
        }

        return bestDropOff;
    }

    std::shared_ptr<model::ResourceNode> findClosestAvailableResource(
        const world::GameWorld& world,
        const model::ResourceNode::ResourceType type,
        const model::Unit& requester) {
        std::shared_ptr<model::ResourceNode> best;
        float bestDistSq = std::numeric_limits<float>::max();

        for (const auto& element : world.getElements()) {
            auto node = std::dynamic_pointer_cast<model::ResourceNode>(element);
            if (!node || node->isDepleted() || node->type() != type) {
                continue;
            }
            if (node->reservedGathererCount() >= node->maxGatherers()) {
                continue;
            }
            const float dSq = distanceSq(requester.getPosition(), node->getPosition());
            if (dSq < bestDistSq) {
                bestDistSq = dSq;
                best = node;
            }
        }
        return best;
    }

    std::shared_ptr<model::Building> findBuilding(
        const world::GameWorld& world, const int teamId, const model::BuildingType type,
        const bool requireComplete) {
        for (const auto& element : world.getElements()) {
            auto building = std::dynamic_pointer_cast<model::Building>(element);
            if (building &&
                building->buildingType() == type &&
                building->getTeamId() == teamId &&
                building->getAction() != model::ActionType::Dead &&
                (!requireComplete || building->isComplete())) {
                return building;
            }
        }
        return nullptr;
    }

    int countBuildings(const world::GameWorld& world, const int teamId, const model::BuildingType type) {
        int count = 0;
        for (const auto& element : world.getElements()) {
            auto building = std::dynamic_pointer_cast<model::Building>(element);
            if (building &&
                building->buildingType() == type &&
                building->getTeamId() == teamId &&
                building->getAction() != model::ActionType::Dead) {
                ++count;
            }
        }
        return count;
    }

    tech::TechState buildTechState(const world::GameWorld& world, const int teamId) {
        tech::TechState state;
        for (const auto& element : world.getElements()) {
            auto building = std::dynamic_pointer_cast<model::Building>(element);
            if (building && building->getTeamId() == teamId &&
                building->isComplete() &&
                building->getAction() != model::ActionType::Dead) {
                state.completedBuildings.insert(building->buildingType());
            }
        }
        // researchedUpgrades stays empty: no upgrade/research content exists yet.
        return state;
    }

    bool hasBuildingRequirements(
        const world::GameWorld& world, const int teamId, const data::BuildingStaticData& data) {
        return tech::TechTreeValidator::canBuild(
            buildTechState(world, teamId), data.buildingType).ok;
    }

    std::shared_ptr<model::Building> firstSelectedBuilding(const SelectionSystem::SelectedList& selected) {
        for (const auto& weak : selected) {
            if (auto building = std::dynamic_pointer_cast<model::Building>(weak.lock());
                building && building->getAction() != model::ActionType::Dead) {
                return building;
            }
        }
        return nullptr;
    }

    std::shared_ptr<model::Unit> firstSelectedWorker(const SelectionSystem::SelectedList& selected) {
        for (const auto& weak : selected) {
            if (auto unit = std::dynamic_pointer_cast<model::Unit>(weak.lock());
                unit && unit->isWorker() && unit->getAction() != model::ActionType::Dead) {
                return unit;
            }
        }
        return nullptr;
    }
}
