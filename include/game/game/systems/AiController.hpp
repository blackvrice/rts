#pragma once

#include <memory>

#include <nlohmann/json.hpp>

#include "core/model/Vector2D.hpp"

namespace rts::core::model {
    class Unit;
}

namespace rts::core::world {
    class GameWorld;
}

namespace rts::core::manager {
    class MovementSystem;
    class ProductionSystem;

    // Scripted opponent for one AI player slot: keeps a barracks up, trains
    // workers to a cap and soldiers for waves, sends idle workers to gather,
    // answers enemy threats near its buildings, and launches attack waves at the
    // first enemy Town Hall. One instance per AI slot (roadmap M3 replaces the
    // script with real strategy/difficulty).
    class AiController {
    public:
        explicit AiController(int playerId) : m_playerId(playerId) {}

        int playerId() const noexcept { return m_playerId; }

        // One AI step. Does nothing once the match is decided or this player is
        // eliminated. passive keeps the economy running but skips the defense and
        // wave passes.
        void update(world::GameWorld& world, MovementSystem& movement,
                    ProductionSystem& production, float dt, bool passive);

        // Back to the opening state with all cadence timers cleared.
        void reset();

        // Save/load of the build-order state and cadence timers.
        nlohmann::json toJson() const;
        void fromJson(const nlohmann::json& ai);

    private:
        enum class BuildOrderState {
            Opening,
            Gather,
            BuildBarracks,
            ProduceArmy,
            Attack,
            Rebuild
        };

        void updateBuildOrder(world::GameWorld& world, ProductionSystem& production);
        // Economy: train workers (to a cap) and warriors, paying from this
        // player's resource pool (no free units).
        void updateProduction(world::GameWorld& world, float dt);
        // Sends this player's idle workers to gather the nearest available resource.
        void updateWorkers(world::GameWorld& world, float dt);
        void updateDefense(world::GameWorld& world, MovementSystem& movement, float dt);
        // Launches an attack wave once enough idle soldiers have massed, or after a
        // timeout, whichever comes first.
        void updateWaves(world::GameWorld& world, MovementSystem& movement, float dt);
        void emitStateFeedback(world::GameWorld& world);

        // Places a barracks near this player's town hall with an idle worker when
        // affordable and none exists. Returns true when construction started.
        bool tryStartBarracks(world::GameWorld& world, ProductionSystem& production) const;
        // Staging point 35% of the way from our hall toward the first enemy hall.
        model::Vector2D rallyPoint(const world::GameWorld& world) const;
        static std::shared_ptr<model::Unit> findIdleWorker(const world::GameWorld& world, int teamId);
        static int countCombatUnits(const world::GameWorld& world, int teamId, bool idleOnly);

        int m_playerId;
        BuildOrderState m_state { BuildOrderState::Opening };
        BuildOrderState m_lastFeedbackState { BuildOrderState::Opening };
        float m_produceTimer { 0.f };
        float m_gatherTimer { 0.f };
        float m_waveTimer { 0.f };
        float m_defenseTimer { 0.f };
    };
}
