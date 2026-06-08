#pragma once

#include <string>
#include <unordered_map>

#include "core/data/BuildingStaticData.hpp"
#include "core/data/ResourceStaticData.hpp"
#include "core/data/SpriteData.hpp"
#include "core/data/UnitStaticData.hpp"

namespace rts::core::data {
    // Central authority for design-time (static) data. Seeded with the built-in
    // defaults at construction, then optionally overridden by JSON files loaded
    // from a data directory. Lookups by enum (internal id) or string id.
    class DataRegistry {
    public:
        // Process-wide registry. Lazily constructed and seeded with defaults so
        // it is safe to query before any JSON has been loaded.
        static DataRegistry& global();

        // Loads units.json / buildings.json / resources.json from dir. Missing
        // or malformed files are logged and skipped (seeded defaults stay in
        // place); unknown string ids and out-of-range values are logged and the
        // offending entry is skipped. Returns true only if every file parsed
        // without error. Emits a summary line of how many entries were loaded.
        bool loadFromDirectory(const std::string& dir);

        const UnitStaticData& unit(::rts::UnitType type) const;
        const BuildingStaticData& building(::rts::core::model::BuildingType type) const;
        const ResourceStaticData& resource(ResourceType type) const;

        // String id -> typed data; nullptr when the id is unknown.
        const UnitStaticData* unitById(const std::string& id) const;
        const BuildingStaticData* buildingById(const std::string& id) const;
        const ResourceStaticData* resourceById(const std::string& id) const;

        // Sprite/animation clip by key (e.g. "unit.warrior.blue.move",
        // "building.barracks.red", "resource.gold.3"); nullptr when unknown.
        const SpriteClip* sprite(const std::string& key) const;
        // Maps a unit string id to its sprite-set name (e.g. "archer" -> "warrior");
        // returns the id itself when no mapping is defined.
        std::string unitSpriteSet(const std::string& unitId) const;

    private:
        DataRegistry();
        void seedDefaults();
        void seedSprites();

        std::unordered_map<::rts::UnitType, UnitStaticData> m_units;
        std::unordered_map<::rts::core::model::BuildingType, BuildingStaticData> m_buildings;
        std::unordered_map<ResourceType, ResourceStaticData> m_resources;

        std::unordered_map<std::string, ::rts::UnitType> m_unitIds;
        std::unordered_map<std::string, ::rts::core::model::BuildingType> m_buildingIds;
        std::unordered_map<std::string, ResourceType> m_resourceIds;

        std::unordered_map<std::string, SpriteClip> m_sprites;
        std::unordered_map<std::string, std::string> m_unitSpriteSets;
    };
}
