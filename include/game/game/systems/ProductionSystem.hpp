#pragma once

#include <optional>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/model/UnitType.hpp"
#include "core/model/Vector2D.hpp"

namespace rts::core::model {
    class Building;
    enum class BuildingType;
}

namespace rts::core::world {
    class GameWorld;
}

namespace rts::core::manager {
    class MovementSystem;

    // Unit production: train/cancel orders on buildings and spawning finished
    // units. Buildings report finished units through a callback; spawns are
    // buffered and flushed after the per-tick element sweep, since adding
    // elements mid-iteration would invalidate the world's element vector.
    class ProductionSystem {
    public:
        // Hooks the building's "unit finished" callback into this system. Must
        // be called for every building added to the world (map, build, load).
        void registerBuildingSpawn(model::Building& building);

        // Spawns buffered units next to their building, then sends them to the
        // rally point (attack-move when an enemy is standing on it).
        void flushPendingSpawns(world::GameWorld& world, MovementSystem& movement);

        // Player train order on a building. unitTypeId < 0 trains the building's
        // default unit. Checks tech, queue room and cost, then pays.
        void train(world::GameWorld& world, model::Building& building, int unitTypeId) const;

        // Removes the last queued unit and refunds its cost to the owner.
        void cancelLast(world::GameWorld& world, model::Building& building) const;

        // Drops buffered spawns (match restart / load).
        void clear() { m_pendingSpawns.clear(); }

        // Save/load of units that finished training but are still waiting for a
        // free tile next to their building.
        nlohmann::json saveState() const;
        void restoreState(const nlohmann::json& state);

        // First entry of the building's data-driven produces list.
        static ::rts::UnitType defaultUnitFor(model::BuildingType type);

    private:
        struct PendingSpawn {
            ::rts::UnitType type;
            model::Vector2D anchor;
            model::Vector2D rally;
            bool hasRally;
            int team;
        };

        // Nearest free tile around the anchor; when a preferred world point is
        // given (the rally point), the closest free tile in that direction wins.
        // Returns nullopt when every tile in range is blocked so the caller can wait.
        static std::optional<model::Vector2D> findFreeSpawnPosition(
            const world::GameWorld& world,
            const model::Vector2D& anchor,
            const std::optional<model::Vector2D>& prefer);

        // True when a live opposing-team element sits near the point (rally on foe).
        static bool isEnemyNear(const world::GameWorld& world, const model::Vector2D& point, int team);

        std::vector<PendingSpawn> m_pendingSpawns;
    };
}
