#pragma once

namespace rts::core::model {
    namespace PlayerId {
        constexpr int Local = 1;
        constexpr int Enemy = 2;
    }

    // Production/build cost expressed in the three economy currencies.
    // food is a population reservation checked against capacity, not a stockpile.
    struct Cost {
        int gold { 0 };
        int wood { 0 };
        int food { 0 };
    };

    struct PlayerResourceState {
        int gold { 1500 };
        int wood { 520 };
        int foodUsed { 24 };
        int foodCapacity { 32 };
        int army { 142 };

        bool canAfford(const Cost& cost) const {
            return gold >= cost.gold &&
                   wood >= cost.wood &&
                   foodUsed + cost.food <= foodCapacity;
        }

        void pay(const Cost& cost) {
            gold -= cost.gold;
            wood -= cost.wood;
            foodUsed += cost.food;
        }

        void refund(const Cost& cost) {
            gold += cost.gold;
            wood += cost.wood;
            foodUsed -= cost.food;
            if (foodUsed < 0) foodUsed = 0;
        }
    };
}
