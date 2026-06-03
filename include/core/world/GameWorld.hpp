//
// Created by black on 26. 2. 1..
//

#pragma once

#include <memory>
#include <vector>

namespace rts::core::map {
    struct TileMapSoA;
}

namespace rts::core::manager {
    class PathManager;
}

namespace rts::core::model {
    class IElement;
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

        const std::vector<std::shared_ptr<model::IElement>>& getElements() const;

        manager::PathManager& path();
        const manager::PathManager& path() const;

        void onCollisionChanged();

    private:
        std::unique_ptr<map::TileMapSoA> m_tileMap;
        std::vector<std::shared_ptr<model::IElement>> m_elements;

        std::unique_ptr<GameWorldGridQuery> m_gridQuery;
        std::unique_ptr<manager::PathManager> m_pathManager;
    };

} // namespace rts::core::world
