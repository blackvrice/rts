//
// Created by black on 26. 2. 1..
//

#pragma once

#include <memory>
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include "core/data/CombatTypes.hpp"
#include "core/ecs/EntityManager.hpp"
#include "core/map/FogOfWar.hpp"
#include "core/model/PlayerResourceState.hpp"
#include "core/sim/SimClock.hpp"
#include "core/world/GridTransform.hpp"

namespace rts::core::map {
    struct TileMapSoA;
}

namespace rts::core::manager {
    class PathManager;
}

namespace rts::core::model {
    class IElement;
    class IGameElement;
    class Projectile;
}

namespace rts::core::path {
    class IGridQuery;
}

namespace rts::core::world {
    class GameWorldGridQuery;

    enum class GameResult {
        InProgress,
        Victory,
        Defeat
    };

    class GameWorld {
    public:
        using ReadLock = std::shared_lock<std::shared_mutex>;
        using WriteLock = std::unique_lock<std::shared_mutex>;

        GameWorld();
        ~GameWorld();

        [[nodiscard]] ReadLock acquireReadLock() const;
        [[nodiscard]] WriteLock acquireWriteLock();

        void addElement(const std::shared_ptr<model::IElement>& element);
        // Clears all dynamic state (elements, entity handles, tick, result) so the
        // caller can repopulate a fresh match. Keeps the tile map. Caller holds the
        // write lock.
        void resetForNewMatch();

        // (Re)sizes the terrain grid and sets the world tile size, e.g. when a map
        // is loaded. Resets all tiles to walkable. Caller holds the write lock.
        void initTileMap(int width, int height, float tileSize);
        // Forces a tile walkable/blocked (blocked tiles have move cost 0).
        void setTileBlocked(int x, int y, bool blocked = true);

        // Ranged attacks spawn projectiles here; the logic tick advances them and
        // drops the ones that hit/expire. Caller holds the write lock.
        void spawnProjectile(const std::shared_ptr<model::Projectile>& projectile);
        void spawnProjectile(model::Vector2D origin, model::IGameElement* target,
                             float damage, data::WeaponType weapon, int ownerTeam,
                             data::SplashRadii splash);
        const std::vector<std::shared_ptr<model::Projectile>>& projectiles() const;
        void updateProjectiles(float dt);

        int gridWidth() const noexcept;
        int gridHeight() const noexcept;

        bool isTileBlocked(int x, int y) const noexcept;
        float tileMoveCost(int x, int y) const noexcept;
        bool isStructureCellOccupied(int x, int y) const noexcept;
        bool isStructurePathBlocked(int x, int y) const noexcept;
        bool isUnitCellOccupied(int x, int y) const noexcept;
        bool isCellOccupied(int x, int y) const noexcept;
        uint64_t collisionVersion() const noexcept;
        const path::IGridQuery& gridQuery() const noexcept;
        const GridTransform& gridTransform() const noexcept;

        const std::vector<std::shared_ptr<model::IElement>>& getElements() const;

        // EntityId handle layer. Callers must already hold a world lock (these do
        // not lock, matching getElements()).
        // True only while the handle's current occupant is live (generation match)
        // and not Dead.
        bool isAlive(ecs::EntityId id) const;
        // Resolves a handle to its live element, or nullptr if stale/dead.
        std::shared_ptr<model::IGameElement> resolve(ecs::EntityId id) const;
        // Destroys EntityIds whose elements have died, so reused slots bump
        // generation and stale handles stop validating. Call once per logic tick.
        void pruneDeadEntities();

        const model::PlayerResourceState& playerResources(int playerId) const;
        void setPlayerResources(int playerId, const model::PlayerResourceState& resources);

        GameResult gameResult() const noexcept { return m_gameResult; }
        void setGameResult(GameResult result) noexcept { m_gameResult = result; }

        // Monotonic logic-tick index; advanced once per fixed simulation tick.
        sim::TickCount currentTick() const noexcept { return m_currentTick; }
        void advanceTick() noexcept { ++m_currentTick; }
        void setCurrentTick(sim::TickCount tick) noexcept { m_currentTick = tick; }

        manager::PathManager& path();
        const manager::PathManager& path() const;

        void onCollisionChanged();

        // Fog of war for the local player. updateFog re-derives the current vision
        // each tick from live player units/buildings; fog() exposes the grid for
        // rendering and minimap tinting. Caller holds the write lock for updateFog.
        void updateFog();
        const map::FogOfWar& fog() const noexcept { return m_fog; }

        // Deterministic 64-bit digest of simulation state (tick, player economy and
        // every entity's id/type/quantized position/hp/action/team), excluding
        // rendering/UI. Equal states hash equal; used by replay verification and the
        // debug overlay. Caller holds at least a read lock.
        std::uint64_t worldHash() const;

    private:
        // Area-of-effect damage around a splash impact; opposing team only, with
        // distance falloff and weapon/armor scaling. Used by splashing projectiles.
        void applySplashDamage(const model::Vector2D& center, float damage,
                               data::WeaponType weapon, int ownerTeam,
                               const data::SplashRadii& splash);

        // Rebuilds the cached structure-occupancy grid (building/resource footprints)
        // so multi-tile walkability is an O(1) lookup instead of a per-query scan.
        void rebuildStructureOccupancy();

        std::unique_ptr<map::TileMapSoA> m_tileMap;
        // gridW*gridH, row-major: 1 where a live building/resource footprint sits.
        std::vector<std::uint8_t> m_structureOccupancy;
        // Same layout, inflated by unit radius for pathfinding clearance.
        std::vector<std::uint8_t> m_structurePathBlocking;
        map::FogOfWar m_fog;  // local player's vision (explored/visible)
        std::vector<std::shared_ptr<model::IElement>> m_elements;
        std::vector<std::shared_ptr<model::Projectile>> m_projectiles;
        std::vector<std::shared_ptr<model::Projectile>> m_projectilePool;
        ecs::EntityManager m_entities;
        std::unordered_map<std::uint32_t, std::weak_ptr<model::IGameElement>> m_entityByIndex;
        std::unordered_map<int, model::PlayerResourceState> m_playerResources;

        std::unique_ptr<GameWorldGridQuery> m_gridQuery;
        std::unique_ptr<manager::PathManager> m_pathManager;
        GridTransform m_gridTransform;
        uint64_t m_collisionVersion { 0 };
        sim::TickCount m_currentTick { 0 };
        GameResult m_gameResult { GameResult::InProgress };
        mutable std::shared_mutex m_mutex;
    };

} // namespace rts::core::world
