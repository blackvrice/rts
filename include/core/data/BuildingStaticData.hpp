#pragma once

#include <string>
#include <vector>

#include "core/data/CombatTypes.hpp"
#include "core/data/TechTree.hpp"
#include "core/model/Building.hpp"
#include "core/model/PlayerResourceState.hpp"
#include "core/model/UnitType.hpp"

namespace rts::core::data {
    struct BuildingStaticData {
        ::rts::core::model::BuildingType buildingType { ::rts::core::model::BuildingType::Barracks };
        std::string displayName { "Building" };
        // HUD selection portrait (Tiny Swords-relative path); empty = generic fallback.
        std::string portrait {};
        float maxHp { 800.0f };
        // Footprint measured in grid tiles (square placement region).
        int footprintWidth { 3 };
        int footprintHeight { 3 };
        // Reveal radius authored in map tiles. A non-positive value keeps the
        // legacy footprint-derived vision fallback for old or partial data files.
        float sightRange { 0.0f };
        float buildTimeSeconds { 20.0f };
        int goldCost { 0 };
        int woodCost { 0 };
        // Unit types this building can train; produces.front() is the default.
        std::vector<::rts::UnitType> produces {};
        // Population capacity contributed while completed.
        int providesSupply { 0 };
        // Whether workers can return gathered resources here.
        bool isDropOff { false };
        // Building types that must exist (completed) before this can be built.
        std::vector<::rts::core::model::BuildingType> requirements {};
        // Upgrades that must be researched before this can be built (none yet).
        std::vector<UpgradeType> requiredUpgrades {};
        // Defensive class for weapon/armor effectiveness (buildings are fortified).
        ArmorType armorType { ArmorType::Fortified };

        ::rts::core::model::Cost cost() const {
            return { goldCost, woodCost, 0 };
        }
    };

    inline BuildingStaticData townHallStaticData() {
        return BuildingStaticData {
            .buildingType = ::rts::core::model::BuildingType::TownHall,
            .displayName = "Town Hall",
            .portrait = "Buildings/Blue Buildings/Castle.png",
            .maxHp = 1500.0f,
            .footprintWidth = 4,
            .footprintHeight = 4,
            .buildTimeSeconds = 30.0f,
            .goldCost = 400,
            .woodCost = 0,
            .produces = { ::rts::UnitType::Worker },
            .providesSupply = 20,
            .isDropOff = true,
            .requirements = {}
        };
    }

    inline BuildingStaticData barracksStaticData() {
        return BuildingStaticData {
            .buildingType = ::rts::core::model::BuildingType::Barracks,
            .displayName = "Barracks",
            .portrait = "Buildings/Blue Buildings/House1.png",
            .maxHp = 800.0f,
            .footprintWidth = 3,
            .footprintHeight = 3,
            .buildTimeSeconds = 20.0f,
            .goldCost = 150,
            .woodCost = 0,
            .produces = { ::rts::UnitType::Warrior, ::rts::UnitType::Archer, ::rts::UnitType::Marine },
            .providesSupply = 0,
            .isDropOff = false,
            .requirements = { ::rts::core::model::BuildingType::TownHall }
        };
    }

    // Built-in fallback table used to seed the DataRegistry and as a safety net
    // when no JSON override exists for a type.
    inline BuildingStaticData defaultBuildingStaticDataFor(const ::rts::core::model::BuildingType type) {
        switch (type) {
            case ::rts::core::model::BuildingType::TownHall:
                return townHallStaticData();
            case ::rts::core::model::BuildingType::Barracks:
                return barracksStaticData();
        }
        return barracksStaticData();
    }

    // Registry-backed lookup (data/buildings.json overrides the defaults).
    // Defined in DataRegistry.cpp.
    BuildingStaticData buildingStaticDataFor(::rts::core::model::BuildingType type);
}

namespace rts::data {
    using ::rts::core::data::BuildingStaticData;
}
