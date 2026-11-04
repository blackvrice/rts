#pragma once

#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

#include "core/model/PlayerSlot.hpp"

namespace rts::core::app {
    // Per-match settings handed to GameLogicManager when a Game scene starts.
    // The lobby will fill this in once skirmish setup exists (roadmap M4-E1-03);
    // until then it comes from SessionContext defaults plus environment overrides.
    struct MatchConfig {
        // Scenario to load. Empty means GameLogicManager::defaultMapPath().
        std::string mapPath;

        // Debug/showcase switch: the enemy AI still runs its economy but never
        // attacks (no waves, no defensive sorties, no idle auto-acquire). Recorded
        // in replays and saves so playback simulates the same rules.
        bool enemyAiPassive { false };

        // Record the live match and auto-save the replay to AppData when it ends.
        // Headless tests turn this off so they never write files.
        bool recordReplay { true };

        // Player slots. Empty means "derive from the map": every player id the
        // map uses becomes a slot in its own alliance (FFA), the local player is
        // Human and the rest are AI. A config may instead list slots explicitly
        // (teams/alliances, all-AI matches, ...). Map ids without a slot here are
        // still added as AI in their own alliance.
        std::vector<model::PlayerSlot> players;

        // Whose view the UI shows (HUD, fog, victory/defeat). Default player 1.
        int localPlayerId { 1 };

        // Folder for the F5/F9 quick-save and the F6/F7 replay slot. Empty means
        // <data>/saves. Tests point it at a temp folder so they never overwrite a
        // developer's own save files.
        std::string saveDirectory;

        // Defaults overridden by environment variables (developer convenience):
        //   RTS_PASSIVE_AI=1      -> enemyAiPassive = true
        //   RTS_MAP=<path>        -> mapPath = <path>
        static MatchConfig fromEnvironment() {
            MatchConfig config;
            if (const char* passive = std::getenv("RTS_PASSIVE_AI"); passive && *passive) {
                const std::string_view value(passive);
                config.enemyAiPassive = value != "0" && value != "false";
            }
            if (const char* map = std::getenv("RTS_MAP"); map && *map) {
                config.mapPath = map;
            }
            return config;
        }
    };
}
