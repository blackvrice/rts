#include "core/world/GameWorld.hpp"

#include "core/manager/PathManager.hpp"
#include "core/map/TileMapSoA.hpp"
#include "core/model/IElement.hpp"
#include "core/world/GameWorldGridQuery.hpp"

namespace rts::core::world {
    GameWorld::GameWorld()
        : m_tileMap(std::make_unique<map::TileMapSoA>())
        , m_gridQuery(std::make_unique<GameWorldGridQuery>(*this))
        , m_pathManager(std::make_unique<manager::PathManager>()) {
    }

    GameWorld::~GameWorld() = default;

    void GameWorld::addElement(const std::shared_ptr<model::IElement>& element) {
        m_elements.push_back(element);
    }

    int GameWorld::gridWidth() const noexcept {
        return m_tileMap->width;
    }

    int GameWorld::gridHeight() const noexcept {
        return m_tileMap->height;
    }

    bool GameWorld::isTileBlocked(int x, int y) const noexcept {
        if (x < 0 || y < 0 || x >= gridWidth() || y >= gridHeight()) {
            return true;
        }

        return m_tileMap->getMoveCost(x, y) == 0;
    }

    bool GameWorld::isCellOccupied(int, int) const noexcept {
        return false;
    }

    const std::vector<std::shared_ptr<model::IElement>>& GameWorld::getElements() const {
        return m_elements;
    }

    manager::PathManager& GameWorld::path() {
        return *m_pathManager;
    }

    const manager::PathManager& GameWorld::path() const {
        return *m_pathManager;
    }

    void GameWorld::onCollisionChanged() {
        m_pathManager->bumpCollisionVersion();
    }
}
