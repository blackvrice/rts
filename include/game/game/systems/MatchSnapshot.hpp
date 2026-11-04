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
    // v3: player slots ("slots", "localPlayer"), every slot's economy, and one AI
    //     state per AI player ("ais"). v2 had a fixed 1v1 and a single "ai".
    inline constexpr int kSaveVersion = 3;

    // Serializes the live match: header (map, AI mode, tick, result, hash), the
    // player slots, the AI states (opaque array from the AI controllers), each
    // player's economy, and every live entity with its runtime state. Entity
    // references are written as the saved EntityIds and remapped on load. Caller
    // holds at least a world read lock.
    nlohmann::json capture(const world::GameWorld& world,
                           const std::string& mapPath,
                           bool enemyAiPassive,
                           const nlohmann::json& aiStates);

    // Restores player slots (v2 saves: the classic human 1 vs AI 2), economies
    // and entities from a snapshot into a world that was already cleared and
    // given its terrain. onBuilding runs for every building right after it is
    // constructed (before it joins the world), so the caller can hook production
    // spawns. Also reads pre-v2 saves (flat buildings/units/resources lists).
    // Caller holds the world write lock.
    void restore(world::GameWorld& world,
                 const nlohmann::json& doc,
                 const std::function<void(model::Building&)>& onBuilding);
}
