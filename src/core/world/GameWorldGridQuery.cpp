// src/core/world/GameWorldGridQuery.cpp
#include "core/world/GameWorldGridQuery.hpp"
#include "core/world/GameWorld.hpp"   // 너 프로젝트의 실제 경로에 맞춰

namespace rts::core::world {

    int GameWorldGridQuery::width() const { return m_world.gridWidth(); }
    int GameWorldGridQuery::height() const { return m_world.gridHeight(); }

    bool GameWorldGridQuery::inBounds(path::GridPos p) const {
        return p.x >= 0 && p.y >= 0 && p.x < width() && p.y < height();
    }

    // 타일 충돌(벽/물 등) 같은 정적 막힘
    bool GameWorldGridQuery::isBlockedStatic(path::GridPos p) const {
        if (!inBounds(p)) return true;
        return m_world.isTileBlocked(p.x, p.y);
    }

    // 유닛/구조물 등 동적 막힘(선택)
    bool GameWorldGridQuery::isBlockedDynamic(path::GridPos p) const {
        if (!inBounds(p)) return true;
        return m_world.isCellOccupied(p.x, p.y);
    }
} // namespace
