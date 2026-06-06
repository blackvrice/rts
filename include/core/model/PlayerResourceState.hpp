#pragma once

namespace rts::core::model {
    namespace PlayerId {
        constexpr int Local = 1;
    }

    struct PlayerResourceState {
        int gold { 1500 };
        int wood { 520 };
        int foodUsed { 24 };
        int foodCapacity { 32 };
        int army { 142 };
    };
}
