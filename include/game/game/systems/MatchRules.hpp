#pragma once

namespace rts::core::world {
    class GameWorld;
}

namespace rts::core::manager {
    // Win/loss rules. A player with no live Town Hall is eliminated (roadmap
    // M1-E6 replaces this with "all buildings destroyed"). The match result is
    // seen from the local player: Defeat once every player in its alliance is
    // eliminated, Victory once every enemy player is eliminated. In FFA with
    // several AIs the match continues while at least one enemy remains.
    class MatchRules {
    public:
        // Marks newly eliminated players, then sets the world's game result (and
        // plays the result cue) on the tick the match is decided. Returns true
        // only on that transition so the caller can react once (e.g. auto-save
        // the replay).
        static bool checkVictoryDefeat(world::GameWorld& world);
    };
}
