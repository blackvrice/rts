#include "core/world/GameWorld.hpp"

#include "core/manager/PathManager.hpp"
#include "core/map/TileMapSoA.hpp"
#include "core/model/IElement.hpp"
#include "core/model/IGameElement.hpp"
#include "core/model/Building.hpp"
#include "core/model/ResourceNode.hpp"
#include "core/model/Projectile.hpp"
#include "core/data/DataRegistry.hpp"
#include "core/world/GameWorldGridQuery.hpp"

#include <algorithm>
#include <cmath>

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
            gameElement->setProjectileSpawner(
                [this](const model::Vector2D& origin, model::IGameElement* target,
                       float damage, data::WeaponType weapon, int team,
                       data::SplashRadii splash) {
                    constexpr float kProjectileSpeed = 520.0f;
                    model::Projectile::SplashApplier applySplash;
                    if (splash.any()) {
                        applySplash = [this](const model::Vector2D& center, float dmg,
                                             data::WeaponType wt, int ownerTeam,
                                             data::SplashRadii radii) {
                            applySplashDamage(center, dmg, wt, ownerTeam, radii);
                        };
                    }
                    spawnProjectile(std::make_shared<model::Projectile>(
                        origin, target, damage, team, kProjectileSpeed, weapon,
                        splash, std::move(applySplash)));
                });
        }

        onCollisionChanged();
    }

    void GameWorld::resetForNewMatch() {
        m_elements.clear();
        m_projectiles.clear();
        m_entities = ecs::EntityManager{};
        m_entityByIndex.clear();
        m_currentTick = 0;
        m_gameResult = GameResult::InProgress;
        onCollisionChanged();
    }

    void GameWorld::spawnProjectile(const std::shared_ptr<model::Projectile>& projectile) {
        if (projectile) {
            m_projectiles.push_back(projectile);
        }
    }

    const std::vector<std::shared_ptr<model::Projectile>>& GameWorld::projectiles() const {
        return m_projectiles;
    }

    void GameWorld::updateProjectiles(const float dt) {
        for (const auto& projectile : m_projectiles) {
            projectile->tick(dt);
        }
        std::erase_if(m_projectiles, [](const auto& p) { return p->expired(); });
    }

    void GameWorld::applySplashDamage(const model::Vector2D& center, const float damage,
                                      const data::WeaponType weapon, const int ownerTeam,
                                      const data::SplashRadii& splash) {
        for (const auto& element : m_elements) {
            auto victim = std::dynamic_pointer_cast<model::IGameElement>(element);
            if (!victim || victim->getAction() == model::ActionType::Dead) {
                continue;
            }
            // No friendly fire and no collateral on neutral objects (resources).
            const int team = victim->getTeamId();
            if (team == ownerTeam || team == model::TeamId::Neutral) {
                continue;
            }
            const model::Vector2D pos = victim->getPosition();
            const float dx = pos.x - center.x;
            const float dy = pos.y - center.y;
            const float dist = std::sqrt(dx * dx + dy * dy);
            const float falloff = data::splashFalloff(dist, splash);
            if (falloff <= 0.0f) {
                continue;
            }
            victim->takeDamage(
                damage * falloff * data::damageMultiplier(weapon, victim->armorType()),
                nullptr);
        }
    }

    void GameWorld::initTileMap(const int width, const int height, const float tileSize) {
        m_tileMap->init(width, height);
        m_gridTransform.tileSize = tileSize;
        onCollisionChanged();
    }

    void GameWorld::setTileBlocked(const int x, const int y, const bool blocked) {
        if (x < 0 || y < 0 || x >= gridWidth() || y >= gridHeight()) {
            return;
        }
        m_tileMap->setMoveCost(x, y, blocked ? 0 : 1);
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

            // Static structures (buildings, resource nodes) occupy their whole
            // footprint so pathfinding routes around them instead of cutting
            // through; units occupy a single cell.
            int footprintWidth = 1;
            int footprintHeight = 1;
            if (const auto building = std::dynamic_pointer_cast<model::Building>(element)) {
                const auto& data = data::DataRegistry::global().building(building->buildingType());
                footprintWidth = data.footprintWidth;
                footprintHeight = data.footprintHeight;
            } else if (const auto resource = std::dynamic_pointer_cast<model::ResourceNode>(element)) {
                const auto& data = data::DataRegistry::global().resource(resource->type());
                footprintWidth = data.footprintWidth;
                footprintHeight = data.footprintHeight;
            }

            const int originX = cell.x - footprintWidth / 2;
            const int originY = cell.y - footprintHeight / 2;
            if (x >= originX && x < originX + footprintWidth &&
                y >= originY && y < originY + footprintHeight) {
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
