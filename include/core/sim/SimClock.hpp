#pragma once

#include <chrono>
#include <cstdint>

namespace rts::core::sim {
    // Single source of truth for the simulation's fixed timestep. The logic
    // thread advances the world exactly kLogicTickHz times per second using a
    // constant delta, independent of the (variable) render frame rate. Anything
    // in the simulation that scales by time must use kFixedDeltaSeconds, never a
    // measured wall-clock delta, so a given sequence of ticks is reproducible.
    inline constexpr int kLogicTickHz = 30;

    // Fixed simulation delta passed to every tick(dt). Constant by design.
    inline constexpr float kFixedDeltaSeconds = 1.0f / static_cast<float>(kLogicTickHz);

    // Wall-clock spacing between logic ticks used by the logic thread's pacer.
    inline constexpr std::chrono::milliseconds kLogicTickInterval { 1000 / kLogicTickHz };

    // Monotonic logic-tick index. 0 before the first tick; advanced once per
    // logic tick by the simulation (GameWorld). Useful for deterministic
    // scheduling/replay and for tick-stamping events.
    using TickCount = std::uint64_t;
}
