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
        m_playerResources.emplace(model::PlayerId::Local, model::PlayerResourceState {});
        m_playerResources.emplace(model::PlayerId::Enemy, model::PlayerResourceState {});
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

        // Game elements get an EntityId handle and a resolver so they can refer to
        // targets by id instead of raw pointers.
        if (auto gameElement = std::dynamic_pointer_cast<model::IGameElement>(element)) {
            const ecs::EntityId id = m_entities.create();
            gameElement->setEntityId(id);
            m_entityByIndex[id.index] = gameElement;
            gameElement->setEntityResolver(
                [this](const ecs::EntityId target) -> model::IGameElement* {
                    return resolve(target).get();
                });
        }

        onCollisionChanged();
    }

    void GameWorld::resetForNewMatch() {
        m_elements.clear();
        m_entities = ecs::EntityManager{};
        m_entityByIndex.clear();
        m_currentTick = 0;
        m_gameResult = GameResult::InProgress;
        onCollisionChanged();
    }

    bool GameWorld::isAlive(const ecs::EntityId id) const {
        if (!m_entities.isAlive(id)) {
            return false;
        }
        const auto it = m_entityByIndex.find(id.index);
        if (it == m_entityByIndex.end()) {
            return false;
        }
        const auto element = it->second.lock();
        return element && element->getAction() != model::ActionType::Dead;
    }

    std::shared_ptr<model::IGameElement> GameWorld::resolve(const ecs::EntityId id) const {
        if (!m_entities.isAlive(id)) {
            return nullptr;
        }
        const auto it = m_entityByIndex.find(id.index);
        return it == m_entityByIndex.end() ? nullptr : it->second.lock();
    }

    void GameWorld::pruneDeadEntities() {
        for (const auto& element : m_elements) {
            const auto gameElement = std::dynamic_pointer_cast<model::IGameElement>(element);
            if (!gameElement) {
                continue;
            }
            const ecs::EntityId id = gameElement->entityId();
            if (m_entities.isAlive(id) &&
                gameElement->getAction() == model::ActionType::Dead) {
                m_entities.destroy(id);
                m_entityByIndex.erase(id.index);
            }
        }
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

    float GameWorld::tileMoveCost(int x, int y) const noexcept {
        if (x < 0 || y < 0 || x >= gridWidth() || y >= gridHeight()) {
            return 0.0f;
        }

        return static_cast<float>(m_tileMap->getMoveCost(x, y));
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

    const model::PlayerResourceState& GameWorld::playerResources(const int playerId) const {
        if (const auto it = m_playerResources.find(playerId); it != m_playerResources.end()) {
            return it->second;
        }

        static const model::PlayerResourceState emptyResources {};
        return emptyResources;
    }

    void GameWorld::setPlayerResources(const int playerId, const model::PlayerResourceState& resources) {
        m_playerResources[playerId] = resources;
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
