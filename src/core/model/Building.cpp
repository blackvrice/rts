#include <algorithm>
#include <memory>
#include "core/model/Building.hpp"

namespace rts::core::model {

    static float maxHpFor(BuildingType t) {
        return t == BuildingType::TownHall ? 1500.f : 800.f;
    }

    static float trainTimeFor(BuildingType t) {
        // TownHall trains Workers (slower), Barracks trains combat units
        return t == BuildingType::TownHall ? 12.f : 8.f;
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
        return m_type == BuildingType::TownHall;
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
        if (m_trainQueue.empty() || m_trainTime <= 0.f) return 0.f;
        return m_trainTimer / m_trainTime;
    }

    void Building::tick(float dt) {
        if (m_hp <= 0.f || m_trainQueue.empty()) return;

        m_trainTimer += dt;
        if (m_trainTimer >= m_trainTime) {
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
