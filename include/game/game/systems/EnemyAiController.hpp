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

    // Scripted opponent for TeamId::Enemy: keeps a barracks up, trains workers to
    // a cap and soldiers for waves, sends idle workers to gather, answers threats
    // near its buildings, and launches attack waves at the player's town hall.
    // Roadmap M3 replaces this with per-player AI controllers.
    class EnemyAiController {
    public:
        // One AI step. Does nothing once the match is decided. passive keeps the
        // economy running but skips the defense and wave passes.
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
        // Enemy economy: train workers (to a cap) and warriors, paying from the
        // enemy resource pool (no free units).
        void updateProduction(world::GameWorld& world, float dt);
        // Sends idle enemy workers to gather the nearest available resource.
        void updateWorkers(world::GameWorld& world, float dt);
        void updateDefense(world::GameWorld& world, MovementSystem& movement, float dt);
        // Launches an attack wave once enough idle soldiers have massed, or after a
        // timeout, whichever comes first.
        void updateWaves(world::GameWorld& world, MovementSystem& movement, float dt);
        void emitStateFeedback(world::GameWorld& world);

        // Places a barracks near the enemy town hall with an idle worker when
        // affordable and none exists. Returns true when construction started.
        static bool tryStartBarracks(world::GameWorld& world, ProductionSystem& production);
        // Staging point 35% of the way from the enemy hall toward the player's.
        static model::Vector2D rallyPoint(const world::GameWorld& world);
        static std::shared_ptr<model::Unit> findIdleWorker(const world::GameWorld& world, int teamId);
        static int countCombatUnits(const world::GameWorld& world, int teamId, bool idleOnly);

        BuildOrderState m_state { BuildOrderState::Opening };
        BuildOrderState m_lastFeedbackState { BuildOrderState::Opening };
        float m_produceTimer { 0.f };
        float m_gatherTimer { 0.f };
        float m_waveTimer { 0.f };
        float m_defenseTimer { 0.f };
    };
}
