//
// Created by black on 25. 12. 23..
//

#pragma once

namespace rts::data {

    enum class TileTerrain {
        Ground,
        Water,
        Cliff,
        Forest
    };

    class TileStaticData {
    public:
        TileTerrain terrain;

        bool walkable;
        bool buildable;
        bool blocksVision;

        int defenseBonus;

        ResourceId texture;
    };

}