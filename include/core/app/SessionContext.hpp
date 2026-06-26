#pragma once

#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>

namespace rts::core::app {
    // Process-wide handoff between scenes (the DI scopes are torn down on scene
    // change, so a few cross-scene values live here instead). Currently: the replay
    // the lobby asked the next Game scene to play back, and the replay storage dir.
    class SessionContext {
    public:
        static SessionContext& instance() {
            static SessionContext ctx;
            return ctx;
        }

        void setReplayToPlay(std::string path) { m_replayToPlay = std::move(path); }
        // Returns the requested replay path once, then clears it (so the next live
        // match records normally).
        std::optional<std::string> takeReplayToPlay() {
            auto v = m_replayToPlay;
            m_replayToPlay.reset();
            return v;
        }

        // AppData replay folder (created on demand): %LOCALAPPDATA%/RTS/replays,
        // falling back to ./replays when the env var is unavailable.
        static std::string replaysDir() {
            std::filesystem::path base;
            if (const char* local = std::getenv("LOCALAPPDATA"); local && *local) {
                base = std::filesystem::path(local) / "RTS" / "replays";
            } else if (const char* appdata = std::getenv("APPDATA"); appdata && *appdata) {
                base = std::filesystem::path(appdata) / "RTS" / "replays";
            } else {
                base = std::filesystem::path("replays");
            }
            std::error_code ec;
            std::filesystem::create_directories(base, ec);
            return base.string();
        }

    private:
        std::optional<std::string> m_replayToPlay;
    };
}
