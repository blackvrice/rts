#include <algorithm>
#include <memory>
#include "core/model/Building.hpp"
#include "core/data/DataRegistry.hpp"

namespace rts::core::model {

    static float maxHpFor(BuildingType t) {
        return t == BuildingType::TownHall ? 1500.f : 800.f;
    }

    static float trainTimeFor(BuildingType t) {
        // TownHall trains Workers (slower), Barracks trains combat units
        return t == BuildingType::TownHall ? 12.f : 8.f;
    }

    // Training duration for the unit currently at the front of the queue, taken
    // from its static data; falls back to the building's default when idle.
    float Building::currentTrainTime() const {
        if (m_trainQueue.empty()) return m_trainTime;
        return ::rts::core::data::DataRegistry::global()
            .unit(m_trainQueue.front()).buildTimeSeconds;
    }

    Building::Building(BuildingType type, Vector2D pos, int teamId)
        : m_type(type)
        , m_position(pos)
        , m_maxHp(maxHpFor(type))
        , m_hp(m_maxHp)
        , m_teamId(teamId)
        , m_trainTime(trainTimeFor(type)) {}

    ActionType Building::getAction() const {
        if (m_hp <= 0.f) return ActionType::Dead;
        if (!m_completed) return ActionType::Build;
        return m_trainQueue.empty() ? ActionType::Idle : ActionType::Build;
    }

    std::string Building::displayName() const {
        switch (m_type) {
            case BuildingType::TownHall: return "Town Hall";
            case BuildingType::Barracks: return "Barracks";
        }
        return "Building";
    }

    bool Building::isDropOff() const noexcept {
        return m_completed && ::rts::core::data::DataRegistry::global().building(m_type).isDropOff;
    }

    void Building::beginConstruction(float buildTimeSeconds, float startHp) {
        m_completed = false;
        m_buildTime = buildTimeSeconds > 0.f ? buildTimeSeconds : 1.f;
        m_buildProgress = 0.f;
        m_hp = std::max(1.f, startHp);
        m_trainQueue.clear();
        m_trainTimer = 0.f;
    }

    bool Building::advanceConstruction(float dt) {
        if (m_completed) return false;

        m_buildProgress += dt;
        // Health ramps with progress so a half-built structure shows partial HP.
        const float t = std::min(1.f, m_buildProgress / m_buildTime);
        m_hp = std::max(m_hp, t * m_maxHp);

        if (m_buildProgress >= m_buildTime) {
            m_completed = true;
            m_buildProgress = m_buildTime;
            m_hp = m_maxHp;
            return true;
        }
        return false;
    }

    float Building::buildProgress01() const noexcept {
        if (m_completed) return 1.f;
        if (m_buildTime <= 0.f) return 0.f;
        return std::min(1.f, m_buildProgress / m_buildTime);
    }

    void Building::takeDamage(float amount, IGameElement*) {
        if (m_hp <= 0.f) return;
        m_hp = std::max(0.f, m_hp - amount);
    }

    bool Building::trainUnit(UnitType type) {
        if (static_cast<int>(m_trainQueue.size()) >= kMaxTrainQueue) return false;
        m_trainQueue.push_back(type);
        return true;
    }

    std::optional<UnitType> Building::cancelLastTrain() {
        if (m_trainQueue.empty()) return std::nullopt;
        const UnitType cancelled = m_trainQueue.back();
        m_trainQueue.pop_back();
        // Resetting the timer when the in-progress item was cancelled keeps the next
        // queued unit from inheriting partial progress.
        if (m_trainQueue.empty()) m_trainTimer = 0.f;
        return cancelled;
    }

    void Building::setUnitSpawnFn(UnitSpawnFn fn) {
        m_spawnFn = std::move(fn);
    }

    void Building::setRallyPoint(const Vector2D& point) {
        m_rallyPoint = point;
        m_hasRallyPoint = true;
    }

    UnitType Building::trainQueueAt(int i) const noexcept {
        if (i < 0 || i >= static_cast<int>(m_trainQueue.size())) return UnitType::Warrior;
        return m_trainQueue[static_cast<std::size_t>(i)];
    }

    float Building::trainProgress() const noexcept {
        const float trainTime = currentTrainTime();
        if (m_trainQueue.empty() || trainTime <= 0.f) return 0.f;
        return m_trainTimer / trainTime;
    }

    void Building::tick(float dt) {
        // Construction progress is driven by workers (advanceConstruction), not the
        // building's own tick. An incomplete building cannot train.
        if (m_hp <= 0.f || !m_completed || m_trainQueue.empty()) return;

        m_trainTimer += dt;
        if (m_trainTimer >= currentTrainTime()) {
            m_trainTimer = 0.f;
            const UnitType spawned = m_trainQueue.front();
            m_trainQueue.pop_front();
            if (m_spawnFn) {
                // Anchor just below the building; the spawn callback resolves a free
                // tile and routes the unit to the rally point when one is set.
                const Vector2D anchor{ m_position.x, m_position.y + 88.f };
                m_spawnFn(spawned, anchor, m_rallyPoint, m_hasRallyPoint, m_teamId);
            }
        }
    }

} // namespace rts::core::model
