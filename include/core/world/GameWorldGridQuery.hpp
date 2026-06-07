// src/core/world/GameWorldGridQuery.hpp
#pragma once
#include "core/path/IGridQuery.hpp"

namespace rts::core::world {

    class GameWorld; // 너의 월드 클래스

    class GameWorldGridQuery final : public path::IGridQuery {
    public:
        explicit GameWorldGridQuery(const GameWorld& w) : m_world(w) {}

        int width()  const override;
        int height() const override;

        bool isBlockedStatic(path::GridPos p) const override;
        bool isBlockedDynamic(path::GridPos p) const override;
        float moveCost(path::GridPos p) const override;

        bool inBounds(path::GridPos p) const override;

    private:
        const GameWorld& m_world;
    };

} // namespace rts::core
