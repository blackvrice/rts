#pragma once

#include <map>
#include <string>
#include <vector>

#include "core/model/Building.hpp"
#include "core/model/ResourceNode.hpp"
#include "core/model/UnitType.hpp"
#include "core/model/Vector2D.hpp"
#include "core/path/GridTypes.hpp"

namespace rts::core::map {
    // A loadable scenario: grid size, starting economy, and every entity placed
    // at the start. Replaces the hard-coded world setup so maps live in data.
    struct MapData {
        int width { 32 };
        int height { 32 };
        float tileSize { 64.0f };

        // Starting stockpile per player id (1 = "player", 2 = "enemy", 3+ = "p3"...).
        // Ids missing here start with the defaults below.
        struct StartResources {
            int gold { 500 };
            int wood { 300 };
        };
        std::map<int, StartResources> startResources;
        StartResources startResourcesFor(int playerId) const;
        // Every player id the scenario uses, ascending: always 1 and 2 (the classic
        // 1v1 slots), plus any owner of a placed building/unit or startResources key.
        std::vector<int> playerIds() const;

        struct BuildingSpawn {
            ::rts::core::model::BuildingType type;
            int teamId;  // owner player id (0 = neutral)
            model::Vector2D position;
        };
        struct UnitSpawn {
            ::rts::UnitType type;
            int teamId;  // owner player id (0 = neutral)
            model::Vector2D position;
        };
        struct ResourceSpawn {
            ::rts::core::model::ResourceNode::ResourceType type;
            model::Vector2D position;
        };

        std::vector<BuildingSpawn> buildings;
        std::vector<UnitSpawn> units;
        std::vector<ResourceSpawn> resources;
        std::vector<path::GridPos> blockedTiles;  // tiles forced to non-walkable
    };

    // Built-in fallback scenario (used when the JSON map is missing/invalid).
    MapData defaultMapData();

    // Loads a map from a JSON file; returns defaultMapData() on failure so the
    // game always has a playable scenario.
    MapData loadMap(const std::string& path);
}
