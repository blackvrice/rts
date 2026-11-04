#pragma once

#include <cstdint>
#include <unordered_map>

#include "core/model/IGameElement.hpp"

namespace rts::core::world {
    class GameWorld;
}

namespace rts::core::manager {
    // Turns per-tick state changes into transient sound/effect events: hits
    // (HP dropped), deaths, and completed construction. Purely presentational;
    // it never changes simulation state.
    class FeedbackSystem {
    public:
        // Compares every element against last tick's snapshot, emits feedback for
        // the changes, then stores the new snapshot.
        void capture(world::GameWorld& world);

        void clear() { m_snapshots.clear(); }

    private:
        struct ElementSnapshot {
            float hp { 0.0f };
            bool complete { false };
            model::ActionType action { model::ActionType::Idle };
        };

        // Keyed by full EntityId (index<<32 | generation) so a recycled slot never
        // mis-attributes one unit's HP change to another (which spawned phantom hits).
        std::unordered_map<std::uint64_t, ElementSnapshot> m_snapshots;
    };
}
