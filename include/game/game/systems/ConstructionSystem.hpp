#pragma once

#include <optional>

#include "core/model/Vector2D.hpp"

namespace rts::core::model {
    class Unit;
}

namespace rts::core::world {
    class GameWorld;
}

namespace rts::core::data {
    struct BuildingStaticData;
}

namespace rts::core::manager {
    class MovementSystem;
    class ProductionSystem;

    // Building placement and construction orders. Stateless: everything it needs
    // lives in the world or is passed in.
    class ConstructionSystem {
    public:
        // Checks the whole w x h footprint (top-left origin in tiles) is walkable and free.
        static bool canPlaceBuilding(const world::GameWorld& world, int originX, int originY, int w, int h);

        // World position of a footprint placed at the tile origin: the footprint's
        // center, so the building's occupied cells and sprite match the validated area.
        static model::Vector2D footprintCenter(const world::GameWorld& world,
                                               int originX, int originY,
                                               const data::BuildingStaticData& data);

        // Searches rings 5..14 tiles around center for the first placeable
        // footprint (used by the AI to site buildings near its town hall).
        static std::optional<model::Vector2D> findBuildSiteNear(
            const world::GameWorld& world,
            const model::Vector2D& center,
            const data::BuildingStaticData& data);

        // Player build order: validates tech, placement and cost, creates the
        // construction site centered on the clicked tile, pays, and sends the
        // worker to build it. Clears the worker's queued orders first.
        static void orderWorkerToBuild(world::GameWorld& world,
                                       ProductionSystem& production,
                                       MovementSystem& movement,
                                       model::Unit& worker,
                                       int buildingTypeId,
                                       const model::Vector2D& position);
    };
}
