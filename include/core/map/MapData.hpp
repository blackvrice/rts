#pragma once

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

        int playerGold { 500 };
        int playerWood { 300 };
        int enemyGold { 500 };
        int enemyWood { 300 };

        struct BuildingSpawn {
            ::rts::core::model::BuildingType type;
            int teamId;
            model::Vector2D position;
        };
        struct UnitSpawn {
            ::rts::UnitType type;
            int teamId;
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
