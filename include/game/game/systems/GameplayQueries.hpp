#pragma once

// Read-only world lookups shared by the gameplay systems (economy, production,
// construction, combat, orders, AI, match rules). Extracted from
// GameLogicManager so each system can reuse them without owning the world.
// Every function iterates world elements in insertion order and returns the
// first/closest match exactly as the original member functions did, which
// keeps simulation results (and WorldHash) unchanged.

#include <memory>
#include <optional>

#include "core/model/ResourceNode.hpp"
#include "core/model/Vector2D.hpp"
#include "core/tech/TechTreeValidator.hpp"
#include "game/game/systems/SelectionSystem.hpp"

namespace rts::core::model {
    class Building;
    class IGameElement;
    class Unit;
    enum class BuildingType;
}

namespace rts::core::world {
    class GameWorld;
}

namespace rts::core::data {
    struct BuildingStaticData;
}

namespace rts::core::manager::queries {
    // Radius around a clicked point within which an element counts as the
    // command target (also used for "enemy near the rally point").
    inline constexpr float kCommandPickRadius = 64.0f;
    inline constexpr float kCommandPickRadiusSq = kCommandPickRadius * kCommandPickRadius;

    inline float distanceSq(const model::Vector2D& a, const model::Vector2D& b) {
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        return dx * dx + dy * dy;
    }

    // Neutral (resources, scenery) is never an enemy of anyone.
    bool isOpposingTeam(int attackerTeamId, int targetTeamId);

    bool selectionContains(const SelectionSystem::SelectedList& selected,
                           const model::IGameElement* candidate);

    // Closest live element to a clicked point, skipping already-selected ones
    // (except selected resource nodes, which workers still need as targets).
    std::shared_ptr<model::IGameElement> findCommandTargetAt(
        const world::GameWorld& world,
        const model::Vector2D& target,
        const SelectionSystem::SelectedList& selected);

    std::shared_ptr<model::Building> findClosestDropOffFor(
        const world::GameWorld& world, const model::Unit& worker);

    // Closest non-depleted node of the type that still has a free gather slot.
    std::shared_ptr<model::ResourceNode> findClosestAvailableResource(
        const world::GameWorld& world,
        model::ResourceNode::ResourceType type,
        const model::Unit& requester);

    // First live building of the type for the team (optionally completed only).
    std::shared_ptr<model::Building> findBuilding(
        const world::GameWorld& world, int teamId, model::BuildingType type,
        bool requireComplete = false);

    int countBuildings(const world::GameWorld& world, int teamId, model::BuildingType type);

    // A team's completed buildings (and researched upgrades) for the TechTreeValidator.
    tech::TechState buildTechState(const world::GameWorld& world, int teamId);

    // True when every prerequisite of the building is completed for the team.
    bool hasBuildingRequirements(const world::GameWorld& world, int teamId,
                                 const data::BuildingStaticData& data);

    std::shared_ptr<model::Building> firstSelectedBuilding(const SelectionSystem::SelectedList& selected);
    std::shared_ptr<model::Unit> firstSelectedWorker(const SelectionSystem::SelectedList& selected);
}
