#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/command/LogicCommand.hpp"

namespace rts::core::replay {
    // One recorded player command, stamped with the simulation tick it applied on.
    struct ReplayEntry {
        std::uint64_t tick { 0 };
        nlohmann::json cmd;  // { "t": <type>, ...fields }
    };

    // A recorded match: the initial scenario, the player command stream and
    // per-tick world-hash checkpoints used to detect replay divergence.
    class ReplayLog {
    public:
        void clear();
        void setMapPath(const std::string& path) { m_mapPath = path; }
        const std::string& mapPath() const noexcept { return m_mapPath; }
        void setMetadata(std::string result, std::uint64_t durationTicks);
        const std::string& result() const noexcept { return m_result; }
        std::uint64_t durationTicks() const noexcept { return m_durationTicks; }

        void record(std::uint64_t tick, nlohmann::json cmd);
        void checkpoint(std::uint64_t tick, std::uint64_t hash);

        // Commands recorded for exactly this tick (in record order).
        std::vector<const ReplayEntry*> commandsForTick(std::uint64_t tick) const;
        // Recorded hash for a tick, if any (for divergence comparison).
        std::optional<std::uint64_t> hashForTick(std::uint64_t tick) const;

        std::uint64_t lastTick() const noexcept { return m_lastTick; }
        bool empty() const noexcept { return m_entries.empty(); }

        bool save(const std::string& path) const;
        bool load(const std::string& path);

    private:
        std::string m_mapPath;
        std::string m_result { "in_progress" };
        std::vector<ReplayEntry> m_entries;
        std::vector<std::pair<std::uint64_t, std::uint64_t>> m_checkpoints;
        std::uint64_t m_lastTick { 0 };
        std::uint64_t m_durationTicks { 0 };
    };

    // Command <-> JSON for the recordable player-order commands. serialize returns
    // nullopt for command types that are not part of the replay stream.
    std::optional<nlohmann::json> serializeLogicCommand(const command::LogicCommand& cmd);
    command::LogicCommandPtr deserializeLogicCommand(const nlohmann::json& j);
}
