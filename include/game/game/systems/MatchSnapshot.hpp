#pragma once

#include <functional>
#include <string>

#include <nlohmann/json.hpp>

namespace rts::core::model {
    class Building;
}

namespace rts::core::world {
    class GameWorld;
}

namespace rts::core::manager::snapshot {
    // Save-file format version written by capture().
    inline constexpr int kSaveVersion = 2;

    // Serializes the live match: header (map, AI mode, tick, result, hash), the
    // opaque AI state, each team's economy, and every live entity with its
    // runtime state. Entity references are written as the saved EntityIds and
    // remapped on load. Caller holds at least a world read lock.
    nlohmann::json capture(const world::GameWorld& world,
                           const std::string& mapPath,
                           bool enemyAiPassive,
                           const nlohmann::json& aiState);

    // Recreates team economies and entities from a snapshot into a world that
    // was already cleared and given its terrain. onBuilding runs for every
    // building right after it is constructed (before it joins the world), so the
    // caller can hook production spawns. Also reads pre-v2 saves (flat
    // buildings/units/resources lists). Caller holds the world write lock.
    void restore(world::GameWorld& world,
                 const nlohmann::json& doc,
                 const std::function<void(model::Building&)>& onBuilding);
}
