#pragma once
#include <cmath>

#include "core/model/Vector2D.hpp"
#include "core/path/GridTypes.hpp"

namespace rts::core::world {

    struct GridTransform {
        float tileSize = 32.f; // tmxlite 타일 크기랑 맞추기

        path::GridPos worldToGrid(const model::Vector2D& p) const {
            return {
                static_cast<int>(std::floor(p.x / tileSize)),
                static_cast<int>(std::floor(p.y / tileSize))
            };
        }

        model::Vector2D gridToWorldCenter(const path::GridPos& g) const {
            return { (g.x + 0.5f) * tileSize, (g.y + 0.5f) * tileSize };
        }
    };

} // namespace
