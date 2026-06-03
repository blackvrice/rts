//
// Created by black on 26. 2. 1..
//

#pragma once
#include <vector>
#include <cstdint>
#include <cassert>
#include <cstddef>


namespace rts::core::map {

    struct TileMapSoA {
        int width = 0, height = 0;

        std::vector<uint8_t> terrain;
        std::vector<uint8_t> moveCost;
        std::vector<uint8_t> flags;
        std::vector<uint8_t> heightMap;

        void init(int w, int h) {
            width = w; height = h;
            const std::size_t n = static_cast<std::size_t>(w) * static_cast<std::size_t>(h);

            terrain.assign(n, 0);
            moveCost.assign(n, 1);   // 기본 이동비용 1 같은 식
            flags.assign(n, 0);
            heightMap.assign(n, 0);
        }

        std::size_t idx(int x, int y) const noexcept {
#ifndef NDEBUG
            assert(x >= 0 && x < width);
            assert(y >= 0 && y < height);
#endif
            return static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
                 + static_cast<std::size_t>(x);
        }

        uint8_t getMoveCost(int x, int y) const noexcept {
            return moveCost[idx(x, y)];
        }
        void setMoveCost(int x, int y, uint8_t c) noexcept {
            moveCost[idx(x, y)] = c;
        }

        uint8_t getFlags(int x, int y) const noexcept {
            return flags[idx(x, y)];
        }
        void setFlags(int x, int y, uint8_t f) noexcept {
            flags[idx(x, y)] = f;
        }
        void addFlags(int x, int y, uint8_t mask) noexcept {
            flags[idx(x, y)] |= mask;
        }
        void clearFlags(int x, int y, uint8_t mask) noexcept {
            flags[idx(x, y)] &= static_cast<uint8_t>(~mask);
        }
    };
}
