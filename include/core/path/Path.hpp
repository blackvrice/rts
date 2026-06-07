//
// Created by black on 26. 2. 17..
//
#pragma once
#include <cstdint>

#include "GridTypes.hpp"

namespace rts::core::path {
    struct PathOptions {
        bool allowDiagonal = false;
        bool useDynamicBlocking = true; // 동적 점유 체크 여부
        bool preventDiagonalCornerCutting = true;
        bool useTerrainCost = true;
        int maxExpand = 20000; // 안전장치(노드 확장 제한)
    };

    // 캐시 키: start/goal + "월드 충돌 버전"
    struct CacheKey {
        GridPos s;
        GridPos g;
        uint64_t collisionVersion{};

        friend bool operator==(const CacheKey &a, const CacheKey &b) = default;
    };

    struct CacheKeyHash {
        size_t operator()(const CacheKey &k) const noexcept {
            size_t h1 = GridPosHash{}(k.s);
            size_t h2 = GridPosHash{}(k.g);
            size_t h3 = std::hash<uint64_t>{}(k.collisionVersion);
            return h1 ^ (h2 * 1315423911u) ^ (h3 * 2654435761u);
        }
    };
}
