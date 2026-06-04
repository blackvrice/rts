//
// Created by black on 26. 2. 1..
//

#pragma once

#include <memory>
#include <cstdint>
#include <vector>

#include "core/world/GridTransform.hpp"

namespace rts::core::map {
    struct TileMapSoA;
}

namespace rts::core::manager {
    class PathManager;
}

namespace rts::core::model {
    class IElement;
}

namespace rts::core::path {
    class IGridQuery;
}

namespace rts::core::world {
    class GameWorldGridQuery;

    class GameWorld {
    public:
        GameWorld();
        ~GameWorld();

        void addElement(const std::shared_ptr<model::IElement>& element);

        int gridWidth() const noexcept;
        int gridHeight() const noexcept;

        bool isTileBlocked(int x, int y) const noexcept;
        bool isCellOccupied(int x, int y) const noexcept;
        uint64_t collisionVersion() const noexcept;
        const path::IGridQuery& gridQuery() const noexcept;
        const GridTransform& gridTransform() const noexcept;

        const std::vector<std::shared_ptr<model::IElement>>& getElements() const;

        manager::PathManager& path();
        const manager::PathManager& path() const;

        void onCollisionChanged();

    private:
        std::unique_ptr<map::TileMapSoA> m_tileMap;
        std::vector<std::shared_ptr<model::IElement>> m_elements;

        std::unique_ptr<GameWorldGridQuery> m_gridQuery;
        std::unique_ptr<manager::PathManager> m_pathManager;
        GridTransform m_gridTransform;
        uint64_t m_collisionVersion { 0 };
    };

} // namespace rts::core::world
