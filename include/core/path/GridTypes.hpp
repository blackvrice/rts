// include/rts/path/GridTypes.hpp
#pragma once
#include <vector>

namespace rts::core::path {
    struct GridPos {
        int x{};
        int y{};

        friend bool operator==(const GridPos &a, const GridPos &b) = default;
    };

    using Path = std::vector<GridPos>;

    struct GridPosHash {
        size_t operator()(const GridPos &p) const noexcept {
            // 간단 해시(충분). 더 좋은 해시 써도 됨.
            return (static_cast<size_t>(p.x) << 32) ^ static_cast<size_t>(p.y);
        }
    };
} // namespace rts::path
