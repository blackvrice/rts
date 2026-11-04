#pragma once

#include <cstdint>

namespace rts::core::model {
    // How a player slot is driven.
    enum class PlayerController : std::uint8_t {
        Human,
        Ai
    };

    // Player color, matching the Tiny Swords art variants (Blue/Red/Yellow/
    // Purple/Black folders). Rendering still uses only blue/red sprite sets until
    // roadmap M2-E6 adds the other colors to data/animations.json.
    enum class PlayerColor : std::uint8_t {
        Blue,
        Red,
        Yellow,
        Purple,
        Black
    };

    // One participant in a match. Elements store their owner's slot id in
    // getTeamId() (0 = neutral, 1..N = players; TeamId::Player/Enemy are the
    // default ids 1 and 2). Players that share an alliance are allies: they share
    // vision and never attack each other. FFA = every player in its own alliance.
    struct PlayerSlot {
        int id { 0 };
        int alliance { 0 };
        PlayerController controller { PlayerController::Ai };
        PlayerColor color { PlayerColor::Blue };
        // Set by MatchRules once the player has lost (no Town Hall left).
        bool eliminated { false };
    };

    // Default color per slot id: 1 Blue, 2 Red, 3 Yellow, 4 Purple, 5+ Black.
    inline PlayerColor defaultPlayerColor(const int playerId) {
        switch (playerId) {
            case 1: return PlayerColor::Blue;
            case 2: return PlayerColor::Red;
            case 3: return PlayerColor::Yellow;
            case 4: return PlayerColor::Purple;
            default: return PlayerColor::Black;
        }
    }

    // Placeholder sprite color until per-color sprite sets exist (M2-E6):
    // neutral and player 1 keep the blue art, every other player uses red. For
    // the default 2-player match this is exactly the previous behavior.
    inline bool usesBlueSprites(const int ownerId) {
        return ownerId <= 1;
    }
}
