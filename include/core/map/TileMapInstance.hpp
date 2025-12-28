//
// Created by black on 25. 12. 23..
//

#pragma once

#include <vector>

#include "entity/TileInstance.hpp"

namespace rts::map {
    class TileMapInstance {
    public:
        std::vector<entity::TileInstance> tiles;

        TileInstance& at(int x, int y) {
            return tiles[y * width + x];
        }

    private:
        int width;
    };
}
