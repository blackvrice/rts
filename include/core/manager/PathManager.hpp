// include/rts/path/PathManager.hpp
#pragma once
#include <algorithm>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <optional>
#include <cmath>
#include <cstdint>   // uintptr_t

#include "core/path/GridTypes.hpp"
#include "core/path/IGridQuery.hpp"
#include "core/path/Path.hpp"

namespace rts::core::manager {

    class PathManager {
    public:
        PathManager() = default; // ✅ 더 이상 grid를 ctor로 받지 않음

        // 외부(World)가 충돌 상태가 바뀔 때 올려주는 버전.
        // ✅ singleton PathManager라도 world별로 다른 버전값을 넘길 수 있게 findPath 인자로 받게 함.
        void setCacheEnabled(bool enabled) { m_cacheEnabled = enabled; }
        void clearCache() {
            std::lock_guard<std::mutex> lock(m_cacheMutex);
            m_cache.clear();
        }

        void bumpCollisionVersion();

        // ✅ 핵심: A* 경로 찾기 (grid를 인자로 받는다)
        std::optional<path::Path> findPath(
            const path::IGridQuery& grid,
            uint64_t collisionVersion,
            path::GridPos start,
            path::GridPos goal,
            const path::PathOptions& opt = {}
        ) {
            if (!grid.inBounds(start) || !grid.inBounds(goal)) return std::nullopt;
            if (start == goal) return path::Path{ start };
            if (isBlocked(grid, goal, opt)) return std::nullopt;

            // ✅ 캐시 키: (gridId + start + goal + collisionVersion)
            // The cache is shared, so its accesses are mutex-guarded: many path
            // requests resolve in parallel on worker threads within one logic tick.
            // The A* itself runs outside the lock (it only reads the grid), so worker
            // threads contend only briefly for the cache lookup/insert.
            const CacheKeyEx key{
                gridId(grid),
                start,
                goal,
                collisionVersion,
                opt.allowDiagonal,
                opt.useDynamicBlocking,
                opt.preventDiagonalCornerCutting,
                opt.useTerrainCost
            };
            if (m_cacheEnabled) {
                std::lock_guard<std::mutex> lock(m_cacheMutex);
                auto it = m_cache.find(key);
                if (it != m_cache.end()) return it->second;
            }

            auto result = aStar(grid, start, goal, opt);
            if (result && m_cacheEnabled) {
                std::lock_guard<std::mutex> lock(m_cacheMutex);
                m_cache.emplace(key, *result);
            }
            return result;
        }

    private:
        bool m_cacheEnabled{ true };

        // ---- 캐시 키 확장 (grid 구분용) ----
        struct CacheKeyEx {
            std::uintptr_t gridId{};
            path::GridPos start{};
            path::GridPos goal{};
            uint64_t collisionVersion{};
            bool allowDiagonal{};
            bool useDynamicBlocking{};
            bool preventDiagonalCornerCutting{};
            bool useTerrainCost{};
            bool operator==(const CacheKeyEx& o) const noexcept {
                return gridId == o.gridId
                    && start == o.start
                    && goal == o.goal
                    && collisionVersion == o.collisionVersion
                    && allowDiagonal == o.allowDiagonal
                    && useDynamicBlocking == o.useDynamicBlocking
                    && preventDiagonalCornerCutting == o.preventDiagonalCornerCutting
                    && useTerrainCost == o.useTerrainCost;
            }
        };

        struct CacheKeyExHash {
            size_t operator()(const CacheKeyEx& k) const noexcept {
                // 간단한 해시 합성 (충분)
                size_t h = 0;
                auto mix = [&](size_t v) {
                    h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
                };
                mix(std::hash<std::uintptr_t>{}(k.gridId));
                mix(path::GridPosHash{}(k.start));
                mix(path::GridPosHash{}(k.goal));
                mix(std::hash<uint64_t>{}(k.collisionVersion));
                mix(std::hash<bool>{}(k.allowDiagonal));
                mix(std::hash<bool>{}(k.useDynamicBlocking));
                mix(std::hash<bool>{}(k.preventDiagonalCornerCutting));
                mix(std::hash<bool>{}(k.useTerrainCost));
                return h;
            }
        };

        static std::uintptr_t gridId(const path::IGridQuery& grid) noexcept {
            return reinterpret_cast<std::uintptr_t>(&grid);
        }

        std::unordered_map<CacheKeyEx, path::Path, CacheKeyExHash> m_cache;
        mutable std::mutex m_cacheMutex;

        struct Node {
            path::GridPos p;
            float g{};
            float f{};
        };

        struct NodeCmp {
            bool operator()(const Node& a, const Node& b) const { return a.f > b.f; }
        };

        static bool isBlocked(const path::IGridQuery& grid, path::GridPos p, const path::PathOptions& opt) {
            if (grid.isBlockedStatic(p)) return true;
            if (opt.useDynamicBlocking && grid.isBlockedDynamic(p)) return true;
            return false;
        }

        static bool isDiagonalStep(path::GridPos from, path::GridPos to) {
            return std::abs(to.x - from.x) == 1 && std::abs(to.y - from.y) == 1;
        }

        static float enteringTileCost(
            const path::IGridQuery& grid,
            path::GridPos p,
            const path::PathOptions& opt) {
            if (!opt.useTerrainCost) {
                return 1.0f;
            }

            return std::max(1.0f, grid.moveCost(p));
        }

        static float stepCost(
            const path::IGridQuery& grid,
            path::GridPos from,
            path::GridPos to,
            const path::PathOptions& opt) {
            const float baseCost = opt.allowDiagonal && isDiagonalStep(from, to)
                ? 1.41421356f
                : 1.0f;
            return baseCost * enteringTileCost(grid, to, opt);
        }

        static bool canEnterNeighbor(
            const path::IGridQuery& grid,
            path::GridPos from,
            path::GridPos to,
            const path::PathOptions& opt) {
            if (!grid.inBounds(to)) return false;
            if (isBlocked(grid, to, opt)) return false;

            if (opt.allowDiagonal &&
                opt.preventDiagonalCornerCutting &&
                isDiagonalStep(from, to)) {
                const path::GridPos sideX{ to.x, from.y };
                const path::GridPos sideY{ from.x, to.y };
                // Diagonal movement must not squeeze through two blocking side cells.
                if (isBlocked(grid, sideX, opt) || isBlocked(grid, sideY, opt)) {
                    return false;
                }
            }

            return true;
        }

        static float heuristic(path::GridPos a, path::GridPos b, bool diagonal) {
            int dx = std::abs(a.x - b.x);
            int dy = std::abs(a.y - b.y);
            if (!diagonal) return static_cast<float>(dx + dy);
            int mn = std::min(dx, dy);
            int mx = std::max(dx, dy);
            return static_cast<float>((mx - mn) + mn * 1.41421356f);
        }

        // Dense per-search bookkeeping indexed by tile (y * width + x). A stamp
        // marks which entries belong to the current search, so the buffers are
        // reused across searches without clearing 65k entries each time. The
        // buffers are thread_local because path batches run on worker threads.
        struct SearchScratch {
            std::vector<float> gScore;
            std::vector<int> cameFrom;            // linear index of the parent tile
            std::vector<std::uint32_t> seenStamp;  // == stamp: gScore/cameFrom valid
            std::vector<std::uint32_t> closedStamp; // == stamp: tile is closed
            std::uint32_t stamp { 0 };

            void begin(std::size_t cellCount) {
                if (gScore.size() != cellCount) {
                    gScore.assign(cellCount, 0.0f);
                    cameFrom.assign(cellCount, -1);
                    seenStamp.assign(cellCount, 0);
                    closedStamp.assign(cellCount, 0);
                    stamp = 0;
                }
                if (++stamp == 0) {
                    // Stamp wrapped: invalidate everything once and restart at 1.
                    std::fill(seenStamp.begin(), seenStamp.end(), 0u);
                    std::fill(closedStamp.begin(), closedStamp.end(), 0u);
                    stamp = 1;
                }
            }
        };

        static SearchScratch& searchScratch() {
            thread_local SearchScratch scratch;
            return scratch;
        }

        // Same expansion order and tie-breaking as the original hash-map A*, so
        // paths (and therefore replays/WorldHash) are unchanged; only the
        // bookkeeping storage differs.
        static std::optional<path::Path> aStar(
            const path::IGridQuery& grid,
            path::GridPos start,
            path::GridPos goal,
            const path::PathOptions& opt
        ) {
            const int width = grid.width();
            const int height = grid.height();
            if (width <= 0 || height <= 0) return std::nullopt;

            auto& scratch = searchScratch();
            scratch.begin(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
            const std::uint32_t stamp = scratch.stamp;
            const auto indexOf = [width](path::GridPos p) {
                return p.y * width + p.x;
            };

            std::priority_queue<Node, std::vector<Node>, NodeCmp> open;

            const int startIndex = indexOf(start);
            scratch.gScore[startIndex] = 0.0f;
            scratch.cameFrom[startIndex] = -1;
            scratch.seenStamp[startIndex] = stamp;
            open.push(Node{ start, 0.0f, heuristic(start, goal, opt.allowDiagonal) });

            int expanded = 0;
            static constexpr int kOffsets[8][2] = {
                {1,0},{-1,0},{0,1},{0,-1},     // orthogonal first (original order)
                {1,1},{1,-1},{-1,1},{-1,-1}    // then diagonals
            };
            const int neighborCount = opt.allowDiagonal ? 8 : 4;

            while (!open.empty()) {
                Node cur = open.top();
                open.pop();

                const int curIndex = indexOf(cur.p);
                if (scratch.closedStamp[curIndex] == stamp) continue;
                scratch.closedStamp[curIndex] = stamp;

                if (++expanded > opt.maxExpand) return std::nullopt;

                if (cur.p == goal) {
                    return reconstructPath(scratch, width, start, goal);
                }

                const float curG = scratch.gScore[curIndex];
                for (int i = 0; i < neighborCount; ++i) {
                    const path::GridPos n{ cur.p.x + kOffsets[i][0], cur.p.y + kOffsets[i][1] };
                    if (!canEnterNeighbor(grid, cur.p, n, opt)) continue;
                    const int nIndex = indexOf(n);
                    if (scratch.closedStamp[nIndex] == stamp) continue;

                    const float tentativeG = curG + stepCost(grid, cur.p, n, opt);
                    const bool seen = scratch.seenStamp[nIndex] == stamp;
                    if (!seen || tentativeG < scratch.gScore[nIndex]) {
                        scratch.seenStamp[nIndex] = stamp;
                        scratch.cameFrom[nIndex] = curIndex;
                        scratch.gScore[nIndex] = tentativeG;
                        const float f = tentativeG + heuristic(n, goal, opt.allowDiagonal);
                        open.push(Node{ n, tentativeG, f });
                    }
                }
            }

            return std::nullopt;
        }

        static std::optional<path::Path> reconstructPath(
            const SearchScratch& scratch,
            int width,
            path::GridPos start,
            path::GridPos goal
        ) {
            path::Path path;
            const int startIndex = start.y * width + start.x;
            int cur = goal.y * width + goal.x;
            path.push_back(goal);

            while (cur != startIndex) {
                if (scratch.seenStamp[cur] != scratch.stamp) return std::nullopt;
                cur = scratch.cameFrom[cur];
                if (cur < 0) return std::nullopt;
                path.push_back(path::GridPos{ cur % width, cur / width });
            }

            std::reverse(path.begin(), path.end());
            return path;
        }
    };

} // namespace rts::core::manager
