// include/rts/path/PathManager.hpp
#pragma once
#include <algorithm>
#include <queue>
#include <unordered_map>
#include <unordered_set>
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
        void clearCache() { m_cache.clear(); }

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
            if (isBlocked(grid, goal, opt)) return std::nullopt;
            if (start == goal) return path::Path{ start };

            // ✅ 캐시 키: (gridId + start + goal + collisionVersion)
            if (m_cacheEnabled) {
                CacheKeyEx key{ gridId(grid), start, goal, collisionVersion };
                auto it = m_cache.find(key);
                if (it != m_cache.end()) return it->second;
            }

            auto result = aStar(grid, start, goal, opt);
            if (result && m_cacheEnabled) {
                CacheKeyEx key{ gridId(grid), start, goal, collisionVersion };
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
            bool operator==(const CacheKeyEx& o) const noexcept {
                return gridId == o.gridId
                    && start == o.start
                    && goal == o.goal
                    && collisionVersion == o.collisionVersion;
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
                return h;
            }
        };

        static std::uintptr_t gridId(const path::IGridQuery& grid) noexcept {
            return reinterpret_cast<std::uintptr_t>(&grid);
        }

        std::unordered_map<CacheKeyEx, path::Path, CacheKeyExHash> m_cache;

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

        static float heuristic(path::GridPos a, path::GridPos b, bool diagonal) {
            int dx = std::abs(a.x - b.x);
            int dy = std::abs(a.y - b.y);
            if (!diagonal) return static_cast<float>(dx + dy);
            int mn = std::min(dx, dy);
            int mx = std::max(dx, dy);
            return static_cast<float>((mx - mn) + mn * 1.41421356f);
        }

        static std::optional<path::Path> aStar(
            const path::IGridQuery& grid,
            path::GridPos start,
            path::GridPos goal,
            const path::PathOptions& opt
        ) {
            std::priority_queue<Node, std::vector<Node>, NodeCmp> open;

            std::unordered_map<path::GridPos, float, path::GridPosHash> gScore;
            std::unordered_map<path::GridPos, path::GridPos, path::GridPosHash> cameFrom;
            std::unordered_set<path::GridPos, path::GridPosHash> closed;

            gScore[start] = 0.0f;
            open.push(Node{ start, 0.0f, heuristic(start, goal, opt.allowDiagonal) });

            int expanded = 0;

            auto neighbors = [&](path::GridPos p, std::vector<path::GridPos>& out) {
                out.clear();
                out.reserve(opt.allowDiagonal ? 8 : 4);

                static constexpr int d4[4][2] = { {1,0},{-1,0},{0,1},{0,-1} };
                for (auto& d : d4) out.push_back({ p.x + d[0], p.y + d[1] });

                if (opt.allowDiagonal) {
                    static constexpr int d4d[4][2] = { {1,1},{1,-1},{-1,1},{-1,-1} };
                    for (auto& d : d4d) out.push_back({ p.x + d[0], p.y + d[1] });
                }
            };

            std::vector<path::GridPos> neigh;
            while (!open.empty()) {
                Node cur = open.top();
                open.pop();

                if (closed.contains(cur.p)) continue;
                closed.insert(cur.p);

                if (++expanded > opt.maxExpand) return std::nullopt;

                if (cur.p == goal) {
                    return reconstructPath(cameFrom, start, goal);
                }

                neighbors(cur.p, neigh);
                for (auto n : neigh) {
                    if (!grid.inBounds(n)) continue;
                    if (isBlocked(grid, n, opt)) continue;
                    if (closed.contains(n)) continue;

                    float step = 1.0f;
                    if (opt.allowDiagonal) {
                        int dx = std::abs(n.x - cur.p.x);
                        int dy = std::abs(n.y - cur.p.y);
                        if (dx + dy == 2) step = 1.41421356f;
                    }

                    float tentativeG = gScore[cur.p] + step;

                    auto it = gScore.find(n);
                    if (it == gScore.end() || tentativeG < it->second) {
                        cameFrom[n] = cur.p;
                        gScore[n] = tentativeG;
                        float f = tentativeG + heuristic(n, goal, opt.allowDiagonal);
                        open.push(Node{ n, tentativeG, f });
                    }
                }
            }

            return std::nullopt;
        }

        static std::optional<path::Path> reconstructPath(
            const std::unordered_map<path::GridPos, path::GridPos, path::GridPosHash>& cameFrom,
            path::GridPos start,
            path::GridPos goal
        ) {
            path::Path path;
            path::GridPos cur = goal;
            path.push_back(cur);

            while (cur != start) {
                auto it = cameFrom.find(cur);
                if (it == cameFrom.end()) return std::nullopt;
                cur = it->second;
                path.push_back(cur);
            }

            std::reverse(path.begin(), path.end());
            return path;
        }
    };

} // namespace rts::core::manager
