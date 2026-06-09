#pragma once

#include <string>

#include "core/model/ResourceNode.hpp"

namespace rts::core::data {
    using ResourceType = ::rts::core::model::ResourceNode::ResourceType;

    // Static (design-time) definition for a harvestable resource node. Field
    // names mirror the ResourceNode constructor so values flow straight through.
    struct ResourceStaticData {
        ResourceType resourceType { ResourceType::Gold };
        std::string displayName { "Resource" };
        int initialAmount { 1000 };
        int gatherAmountPerTrip { 10 };
        float gatherDurationSeconds { 1.5f };
        int maxGatherers { 3 };
        // Footprint in grid tiles; pathfinding blocks this whole region so units
        // route around the node instead of through it.
        int footprintWidth { 1 };
        int footprintHeight { 1 };
    };

    inline ResourceStaticData goldResourceStaticData() {
        return ResourceStaticData {
            .resourceType = ResourceType::Gold,
            .displayName = "Gold Mine",
            .initialAmount = 5000,
            .gatherAmountPerTrip = 10,
            .gatherDurationSeconds = 1.5f,
            .maxGatherers = 3,
            .footprintWidth = 2,
            .footprintHeight = 2
        };
    }

    inline ResourceStaticData woodResourceStaticData() {
        return ResourceStaticData {
            .resourceType = ResourceType::Wood,
            .displayName = "Forest",
            .initialAmount = 2000,
            .gatherAmountPerTrip = 10,
            .gatherDurationSeconds = 1.5f,
            .maxGatherers = 3,
            .footprintWidth = 2,
            .footprintHeight = 2
        };
    }

    // Built-in fallback table used to seed the DataRegistry and as a safety net
    // when no JSON override exists for a type.
    inline ResourceStaticData defaultResourceStaticDataFor(const ResourceType type) {
        switch (type) {
            case ResourceType::Gold:
                return goldResourceStaticData();
            case ResourceType::Wood:
                return woodResourceStaticData();
        }
        return goldResourceStaticData();
    }

    // Registry-backed lookup (data/resources.json overrides the defaults).
    // Defined in DataRegistry.cpp.
    ResourceStaticData resourceStaticDataFor(ResourceType type);
}

namespace rts::data {
    using ::rts::core::data::ResourceStaticData;
}
