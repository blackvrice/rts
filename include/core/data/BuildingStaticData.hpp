#pragma once

#include <string>

#include "core/model/Building.hpp"
#include "core/model/PlayerResourceState.hpp"

namespace rts::core::data {
    struct BuildingStaticData {
        ::rts::core::model::BuildingType buildingType { ::rts::core::model::BuildingType::Barracks };
        std::string displayName { "Building" };
        float maxHp { 800.0f };
        // Footprint measured in grid tiles (square placement region).
        int footprintWidth { 3 };
        int footprintHeight { 3 };
        float buildTimeSeconds { 20.0f };
        int goldCost { 0 };
        int woodCost { 0 };

        ::rts::core::model::Cost cost() const {
            return { goldCost, woodCost, 0 };
        }
    };

    inline BuildingStaticData townHallStaticData() {
        return BuildingStaticData {
            .buildingType = ::rts::core::model::BuildingType::TownHall,
            .displayName = "Town Hall",
            .maxHp = 1500.0f,
            .footprintWidth = 4,
            .footprintHeight = 4,
            .buildTimeSeconds = 30.0f,
            .goldCost = 400,
            .woodCost = 0
        };
    }

    inline BuildingStaticData barracksStaticData() {
        return BuildingStaticData {
            .buildingType = ::rts::core::model::BuildingType::Barracks,
            .displayName = "Barracks",
            .maxHp = 800.0f,
            .footprintWidth = 3,
            .footprintHeight = 3,
            .buildTimeSeconds = 20.0f,
            .goldCost = 150,
            .woodCost = 0
        };
    }

    inline BuildingStaticData buildingStaticDataFor(const ::rts::core::model::BuildingType type) {
        switch (type) {
            case ::rts::core::model::BuildingType::TownHall:
                return townHallStaticData();
            case ::rts::core::model::BuildingType::Barracks:
                return barracksStaticData();
        }
        return barracksStaticData();
    }
}

namespace rts::data {
    using ::rts::core::data::BuildingStaticData;
}
