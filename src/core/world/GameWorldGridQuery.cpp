// src/core/world/GameWorldGridQuery.cpp
#include "core/world/GameWorldGridQuery.hpp"
#include "core/world/GameWorld.hpp"   // 너 프로젝트의 실제 경로에 맞춰

namespace rts::core::world {

    int GameWorldGridQuery::width() const { return m_world.gridWidth(); }
    int GameWorldGridQuery::height() const { return m_world.gridHeight(); }

    bool GameWorldGridQuery::inBounds(int x, int y) const {
        return x >= 0 && y >= 0 && x < width() && y < height();
    }

    // 타일 충돌(벽/물 등) 같은 정적 막힘
    bool GameWorldGridQuery::isBlockedStatic(int x, int y) const {
        if (!inBounds(x, y)) return true;
        return m_world.isTileBlocked(x, y);  // ✅ GameWorld에 맞게 함수명 연결
    }

    // 유닛/구조물 등 동적 막힘(선택)
    bool GameWorldGridQuery::isBlockedDynamic(int x, int y) const {
        if (!inBounds(x, y)) return true;
        return m_world.isCellOccupied(x, y); // ✅ 없으면 일단 false로 시작해도 됨
    }


    LONG
} // namespace
