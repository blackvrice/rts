#include "core/world/GameWorld.hpp"

#include "core/manager/PathManager.hpp"
#include "core/map/TileMapSoA.hpp"
#include "core/model/IElement.hpp"
#include "core/model/IGameElement.hpp"
#include "core/world/GameWorldGridQuery.hpp"

namespace rts::core::world {
    GameWorld::GameWorld()
        : m_tileMap(std::make_unique<map::TileMapSoA>())
        , m_gridQuery(std::make_unique<GameWorldGridQuery>(*this))
        , m_pathManager(std::make_unique<manager::PathManager>())
        , m_gridTransform{64.f} {
        m_tileMap->init(32, 32);
    }

    GameWorld::~GameWorld() = default;

    GameWorld::ReadLock GameWorld::acquireReadLock() const {
        return ReadLock(m_mutex);
    }

    GameWorld::WriteLock GameWorld::acquireWriteLock() {
        return WriteLock(m_mutex);
    }

    void GameWorld::addElement(const std::shared_ptr<model::IElement>& element) {
        m_elements.push_back(element);
        onCollisionChanged();
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

    bool GameWorld::isCellOccupied(int x, int y) const noexcept {
        if (x < 0 || y < 0 || x >= gridWidth() || y >= gridHeight()) {
            return true;
        }

        for (const auto& element : m_elements) {
            const auto gameElement = std::dynamic_pointer_cast<model::IGameElement>(element);
            if (!gameElement || gameElement->getAction() == model::ActionType::Dead) {
                continue;
            }

            const auto cell = m_gridTransform.worldToGrid(gameElement->getPosition());
            if (cell.x == x && cell.y == y) {
                return true;
            }
        }

        return false;
    }

    uint64_t GameWorld::collisionVersion() const noexcept {
        return m_collisionVersion;
    }

    const path::IGridQuery& GameWorld::gridQuery() const noexcept {
        return *m_gridQuery;
    }

    const GridTransform& GameWorld::gridTransform() const noexcept {
        return m_gridTransform;
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
        ++m_collisionVersion;
        m_pathManager->bumpCollisionVersion();
    }
}
