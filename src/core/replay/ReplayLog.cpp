#include "core/replay/ReplayLog.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <utility>

namespace rts::core::replay {
    namespace cmd = ::rts::core::command;
    using json = nlohmann::json;

    void ReplayLog::clear() {
        m_entries.clear();
        m_checkpoints.clear();
        m_lastTick = 0;
        m_mapPath.clear();
        m_result = "in_progress";
        m_durationTicks = 0;
    }

    void ReplayLog::setMetadata(std::string result, const std::uint64_t durationTicks) {
        m_result = std::move(result);
        m_durationTicks = durationTicks;
        if (durationTicks > m_lastTick) {
            m_lastTick = durationTicks;
        }
    }

    void ReplayLog::record(const std::uint64_t tick, json command) {
        m_entries.push_back({ tick, std::move(command) });
        if (tick > m_lastTick) m_lastTick = tick;
    }

    void ReplayLog::checkpoint(const std::uint64_t tick, const std::uint64_t hash) {
        m_checkpoints.emplace_back(tick, hash);
        if (tick > m_lastTick) m_lastTick = tick;
    }

    std::vector<const ReplayEntry*> ReplayLog::commandsForTick(const std::uint64_t tick) const {
        std::vector<const ReplayEntry*> out;
        for (const auto& e : m_entries) {
            if (e.tick == tick) out.push_back(&e);
        }
        return out;
    }

    std::optional<std::uint64_t> ReplayLog::hashForTick(const std::uint64_t tick) const {
        for (const auto& [t, h] : m_checkpoints) {
            if (t == tick) return h;
        }
        return std::nullopt;
    }

    bool ReplayLog::save(const std::string& path) const {
        json doc;
        doc["map"] = m_mapPath;
        doc["lastTick"] = m_lastTick;
        doc["metadata"] = {
            { "map", m_mapPath },
            { "result", m_result },
            { "durationTicks", m_durationTicks > 0 ? m_durationTicks : m_lastTick }
        };
        json cmds = json::array();
        for (const auto& e : m_entries) {
            cmds.push_back({ { "tick", e.tick }, { "cmd", e.cmd } });
        }
        doc["commands"] = cmds;
        json cps = json::array();
        for (const auto& [t, h] : m_checkpoints) {
            cps.push_back({ { "tick", t }, { "hash", h } });
        }
        doc["checkpoints"] = cps;

        std::ofstream out(path);
        if (!out) {
            std::cerr << "[Replay] cannot write " << path << "\n";
            return false;
        }
        out << doc.dump(1);
        std::cout << "[Replay] saved " << m_entries.size() << " commands to " << path << "\n";
        return true;
    }

    bool ReplayLog::load(const std::string& path) {
        std::ifstream in(path);
        if (!in) {
            std::cerr << "[Replay] cannot open " << path << "\n";
            return false;
        }
        json doc;
        try {
            in >> doc;
        } catch (const std::exception& e) {
            std::cerr << "[Replay] parse error: " << e.what() << "\n";
            return false;
        }
        clear();
        m_mapPath = doc.value("map", std::string{});
        if (const auto meta = doc.value("metadata", json::object()); !meta.empty()) {
            m_mapPath = meta.value("map", m_mapPath);
            m_result = meta.value("result", m_result);
            m_durationTicks = meta.value("durationTicks", 0ull);
        }
        for (const auto& e : doc.value("commands", json::array())) {
            record(e.value("tick", 0ull), e.value("cmd", json::object()));
        }
        for (const auto& c : doc.value("checkpoints", json::array())) {
            checkpoint(c.value("tick", 0ull), c.value("hash", 0ull));
        }
        m_lastTick = std::max(m_lastTick, doc.value("lastTick", 0ull));
        m_lastTick = std::max(m_lastTick, m_durationTicks);
        std::cout << "[Replay] loaded " << path << " (" << m_entries.size() << " commands)\n";
        return true;
    }

    // ---- command (de)serialization ---------------------------------------

    std::optional<json> serializeLogicCommand(const cmd::LogicCommand& c) {
        if (const auto* m = dynamic_cast<const cmd::MoveCommand*>(&c)) {
            return json{ { "t", "move" }, { "x", m->target().x }, { "y", m->target().y },
                         { "append", m->append() } };
        }
        if (const auto* a = dynamic_cast<const cmd::AttackCommand*>(&c)) {
            return json{ { "t", "attack" }, { "x", a->target().x }, { "y", a->target().y },
                         { "append", a->append() },
                         { "eidx", a->targetEntityId().index },
                         { "egen", a->targetEntityId().generation } };
        }
        if (const auto* am = dynamic_cast<const cmd::AttackMoveCommand*>(&c)) {
            return json{ { "t", "amove" }, { "uid", am->unitId() },
                         { "x", am->target().x }, { "y", am->target().y } };
        }
        if (const auto* p = dynamic_cast<const cmd::PatrolCommand*>(&c)) {
            return json{ { "t", "patrol" }, { "uid", p->unitId() },
                         { "fx", p->from().x }, { "fy", p->from().y },
                         { "tx", p->to().x }, { "ty", p->to().y } };
        }
        if (const auto* s = dynamic_cast<const cmd::StopCommand*>(&c)) {
            return json{ { "t", "stop" }, { "uid", s->unitId() } };
        }
        if (const auto* h = dynamic_cast<const cmd::HoldPositionCommand*>(&c)) {
            return json{ { "t", "hold" }, { "uid", h->unitId() } };
        }
        if (const auto* g = dynamic_cast<const cmd::GatherCommand*>(&c)) {
            return json{ { "t", "gather" }, { "x", g->target().x }, { "y", g->target().y } };
        }
        if (const auto* b = dynamic_cast<const cmd::BuildCommand*>(&c)) {
            return json{ { "t", "build" }, { "bt", b->buildingTypeId() },
                         { "x", b->position().x }, { "y", b->position().y } };
        }
        if (const auto* t = dynamic_cast<const cmd::TrainUnitCommand*>(&c)) {
            return json{ { "t", "train" }, { "bid", t->buildingId() }, { "ut", t->unitTypeId() } };
        }
        if (dynamic_cast<const cmd::CancelProductionCommand*>(&c)) {
            return json{ { "t", "cancel" } };
        }
        if (const auto* sel = dynamic_cast<const cmd::SelectCommand*>(&c)) {
            const auto r = sel->area();
            return json{ { "t", "select" },
                         { "l", r.left() }, { "tp", r.top() }, { "r", r.right() }, { "b", r.bottom() },
                         { "add", sel->additive() }, { "same", sel->sameType() } };
        }
        if (const auto* cg = dynamic_cast<const cmd::ControlGroupSelectCommand*>(&c)) {
            return json{ { "t", "cgsel" }, { "g", cg->groupId() } };
        }
        if (const auto* cg = dynamic_cast<const cmd::ControlGroupAssignCommand*>(&c)) {
            return json{ { "t", "cgassign" }, { "g", cg->groupId() } };
        }
        if (const auto* cg = dynamic_cast<const cmd::ControlGroupAddCommand*>(&c)) {
            return json{ { "t", "cgadd" }, { "g", cg->groupId() } };
        }
        return std::nullopt;  // not part of the replay stream
    }

    cmd::LogicCommandPtr deserializeLogicCommand(const json& j) {
        const std::string t = j.value("t", std::string{});
        const auto vec = [&](const char* kx, const char* ky) {
            return core::model::Vector2D{ j.value(kx, 0.0f), j.value(ky, 0.0f) };
        };
        if (t == "move") {
            return std::make_unique<cmd::MoveCommand>(vec("x", "y"), j.value("append", false));
        }
        if (t == "attack") {
            core::ecs::EntityId id{ j.value("eidx", 0xFFFFFFFFu), j.value("egen", 0u) };
            return std::make_unique<cmd::AttackCommand>(vec("x", "y"), j.value("append", false), id);
        }
        if (t == "amove") {
            return std::make_unique<cmd::AttackMoveCommand>(j.value("uid", -1), vec("x", "y"));
        }
        if (t == "patrol") {
            return std::make_unique<cmd::PatrolCommand>(j.value("uid", -1), vec("fx", "fy"), vec("tx", "ty"));
        }
        if (t == "stop") {
            return std::make_unique<cmd::StopCommand>(j.value("uid", -1));
        }
        if (t == "hold") {
            return std::make_unique<cmd::HoldPositionCommand>(j.value("uid", -1));
        }
        if (t == "gather") {
            return std::make_unique<cmd::GatherCommand>(vec("x", "y"));
        }
        if (t == "build") {
            return std::make_unique<cmd::BuildCommand>(j.value("bt", -1), vec("x", "y"));
        }
        if (t == "train") {
            return std::make_unique<cmd::TrainUnitCommand>(j.value("bid", -1), j.value("ut", -1));
        }
        if (t == "cancel") {
            return std::make_unique<cmd::CancelProductionCommand>(-1);
        }
        if (t == "select") {
            const core::model::Vector2D a{ j.value("l", 0.0f), j.value("tp", 0.0f) };
            const core::model::Vector2D b{ j.value("r", 0.0f), j.value("b", 0.0f) };
            return std::make_unique<cmd::SelectCommand>(a, b, j.value("add", false), j.value("same", false));
        }
        if (t == "cgsel") {
            return std::make_unique<cmd::ControlGroupSelectCommand>(j.value("g", 0));
        }
        if (t == "cgassign") {
            return std::make_unique<cmd::ControlGroupAssignCommand>(j.value("g", 0));
        }
        if (t == "cgadd") {
            return std::make_unique<cmd::ControlGroupAddCommand>(j.value("g", 0));
        }
        return nullptr;
    }
}
