#pragma once

namespace rts::core::world {
    class GameWorld;
}

namespace rts::core::manager {
    // Win/loss rules. Current rule: a side with no live Town Hall loses
    // (roadmap M1-E6 replaces this with "all buildings destroyed").
    class MatchRules {
    public:
        // Sets the world's game result (and plays the result cue) the tick a side
        // loses. Returns true only on that transition so the caller can react
        // once (e.g. auto-save the replay).
        static bool checkVictoryDefeat(world::GameWorld& world);
    };
}
