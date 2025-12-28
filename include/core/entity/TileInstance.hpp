//
// Created by black on 25. 12. 23..
//

#pragma once
#include "data/TileStaticData.hpp"


namespace rts::entity {

    class TileInstance {
    public:
        const data::TileStaticData& staticData; // 🔥 Flyweight

        // runtime state
        bool explored = false;
        bool hasResource = false;

        TileInstance(const data::TileStaticData& data)
            : staticData(data) {}
    };

}
