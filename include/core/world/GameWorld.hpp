//
// Created by black on 26. 2. 1..
//

#pragma once

#include <memory>
#include <cstdint>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/data/CombatTypes.hpp"
#include "core/ecs/EntityManager.hpp"
#include "core/map/FogOfWar.hpp"
#include "core/model/PlayerResourceState.hpp"
#include "core/model/PlayerSlot.hpp"
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

        // Save/load support: restore the entity allocator first, then re-add each
        // saved element (dead ones included, in saved order) with its exact saved
        // EntityId, so handles, hash order and future allocations match the
        // saved match. Caller holds the write lock.
        ecs::EntityManager::State entityState() const { return m_entities.state(); }
        void restoreEntityState(ecs::EntityManager::State state);
        void restoreElement(const std::shared_ptr<model::IElement>& element, ecs::EntityId id);
        // Re-creates an in-flight projectile without the firing sound/effect.
        void restoreProjectile(model::Vector2D position, model::IGameElement* target,
                               model::Vector2D lastKnownTargetPosition, float angleDeg,
                               float damage, int ownerTeam, float speed,
                               data::WeaponType weapon, data::SplashRadii splash,
                               const std::string& texturePath);
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
                             data::SplashRadii splash, const std::string& texturePath = {});
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

        // Match participants. Every element's getTeamId() is its owner's slot id
        // (0 = neutral). The local player is whose view the UI shows (HUD, fog,
        // victory/defeat); it may be AI-controlled when spectating. Setting the
        // players resets every player's fog. Caller holds the write lock.
        void setPlayers(std::vector<model::PlayerSlot> players, int localPlayerId);
        const std::vector<model::PlayerSlot>& players() const noexcept { return m_players; }
        const model::PlayerSlot* player(int playerId) const;
        model::PlayerSlot* player(int playerId);
        int localPlayerId() const noexcept { return m_localPlayerId; }
        // Different non-neutral owners in different alliances. Unknown ids count
        // as their own alliance, so two unknown players are enemies.
        bool areEnemies(int a, int b) const;
        // Same owner or same alliance (never neutral).
        bool areAllies(int a, int b) const;
        bool isAiControlled(int playerId) const;

        const model::PlayerResourceState& playerResources(int playerId) const;
        void setPlayerResources(int playerId, const model::PlayerResourceState& resources);

        GameResult gameResult() const noexcept { return m_gameResult; }
        void setGameResult(GameResult result) noexcept { m_gameResult = result; }

        // True while a replay is being played back: the UI reads this (like
        // gameResult) to ignore live player input so the viewer can only watch.
        bool isReplayActive() const noexcept { return m_replayActive; }
        void setReplayActive(bool active) noexcept { m_replayActive = active; }

        // Monotonic logic-tick index; advanced once per fixed simulation tick.
        sim::TickCount currentTick() const noexcept { return m_currentTick; }
        void advanceTick() noexcept { ++m_currentTick; }
        void setCurrentTick(sim::TickCount tick) noexcept { m_currentTick = tick; }

        manager::PathManager& path();
        const manager::PathManager& path() const;

        void onCollisionChanged();

        // Fog of war per player. updateFog re-derives every player's current
        // vision each tick from the live units/buildings of that player and its
        // allies (shared vision); fog() is the local player's grid used for
        // rendering and minimap tinting. Caller holds the write lock for updateFog.
        void updateFog();
        const map::FogOfWar& fog() const noexcept { return fog(m_localPlayerId); }
        const map::FogOfWar& fog(int playerId) const;

        // Currently-seeing sources of the local player and its allies, in world coordinates,
        // refreshed every updateFog. The renderer uses these to draw the visible
        // area as smooth circles; the tile fog grid still drives gameplay (enemy
        // hiding) and the explored/unexplored memory.
        struct VisionSource { model::Vector2D center; float radius; };  // radius in world units
        const std::vector<VisionSource>& visionSources() const noexcept { return m_visionSources; }

        // Deterministic 64-bit digest of simulation state (tick, player economy and
        // every entity's id/type/quantized position/hp/action/team), excluding
        // rendering/UI. Equal states hash equal; used by replay verification and the
        // debug overlay. Caller holds at least a read lock.
        std::uint64_t worldHash() const;

    private:
        // Injects the world services (entity resolver, projectile spawner, alliance
        // query, building hit radius) into a game element joining the world.
        void attachGameElement(const std::shared_ptr<model::IGameElement>& gameElement);

        // Area-of-effect damage around a splash impact; opposing team only, with
        // distance falloff and weapon/armor scaling. Used by splashing projectiles.
        void applySplashDamage(const model::Vector2D& center, float damage,
                               data::WeaponType weapon, int ownerTeam,
                               const data::SplashRadii& splash);

        // Rebuilds the cached structure-occupancy grid (building/resource footprints)
        // so multi-tile walkability is an O(1) lookup instead of a per-query scan.
        void rebuildStructureOccupancy();
        // Rebuilds the cached unit-occupancy grid. A* asks about dynamic blocking
        // for many cells, so this must be O(1) at query time.
        void rebuildUnitOccupancy();

        std::unique_ptr<map::TileMapSoA> m_tileMap;
        // gridW*gridH, row-major: 1 where a live building/resource footprint sits.
        std::vector<std::uint8_t> m_structureOccupancy;
        // Same layout, inflated by unit radius for pathfinding clearance.
        std::vector<std::uint8_t> m_structurePathBlocking;
        // gridW*gridH, row-major: 1 where a live unit's current center cell sits.
        std::vector<std::uint8_t> m_unitOccupancy;
        std::vector<model::PlayerSlot> m_players;
        int m_localPlayerId { 1 };  // TeamId::Player (default human slot)
        std::map<int, map::FogOfWar> m_fogs;  // per player id (explored/visible)
        map::FogOfWar m_emptyFog;             // returned for unknown players
        std::vector<VisionSource> m_visionSources;  // smooth-fog reveal circles (world coords)
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
        bool m_replayActive { false };
        mutable std::shared_mutex m_mutex;
    };

} // namespace rts::core::world
