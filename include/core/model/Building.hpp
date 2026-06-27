#pragma once
#include <deque>
#include <functional>
#include <optional>
#include <string>
#include <memory>
#include <vector>
#include "IGameElement.hpp"
#include "UnitType.hpp"

namespace rts::core::model {

    enum class BuildingType {
        TownHall,
        Barracks
    };

    class Building : public IGameElement {
    public:
        // Spawn callback supplies the trained unit type, the building anchor (caller
        // resolves a free tile around it), the rally destination, whether a rally was
        // set, and the owning team.
        using UnitSpawnFn = std::function<void(UnitType, const Vector2D& anchor,
                                               const Vector2D& rally, bool hasRally, int team)>;
        static constexpr int kMaxTrainQueue = 5;

        struct RuntimeState {
            std::vector<UnitType> trainQueue;
            float trainTimer { 0.0f };
            Vector2D rallyPoint {};
            bool hasRallyPoint { false };
            bool completed { true };
            float buildTime { 0.0f };
            float buildProgress { 0.0f };
        };

        Building(BuildingType type, Vector2D pos, int teamId);

        // ===== IGameElement / IElement interface =====
        void tick(float dt) override;
        void update() override {}
        const GameState& state() const override { return m_state; }
        Vector2D getPosition() const override { return m_position; }
        void setPosition(const Vector2D& pos) override { m_position = pos; }

        void idle() override {}
        void stop() override {}
        void moveTo(const Vector2D&) override {}
        void patrol(const Vector2D&, const Vector2D&) override {}
        void holdPosition() override {}
        void attack(IGameElement*) override {}
        void attackMove(const Vector2D&) override {}
        void takeDamage(float amount, IGameElement* attacker) override;
        void gather(IGameElement*) override {}
        void build(int, const Vector2D&) override {}
        void cast(int, const Vector2D&) override {}
        void setSelected(bool selected) override { m_state.selected = selected; }
        int getTeamId() const override { return m_teamId; }
        void setTeamId(int teamId) override { m_teamId = teamId; }

        ActionType getAction() const override;

        // ===== Custom members =====
        BuildingType buildingType() const { return m_type; }
        bool isDropOff() const noexcept;
        core::data::ArmorType armorType() const override;
        // World-unit reach added to an attacker's range so units can hit the
        // building from its footprint edge instead of needing to reach its center.
        // Set by GameWorld::addElement once the map tile size is known.
        float attackHitRadius() const noexcept override { return m_attackHitRadius; }
        void setAttackHitRadius(float radius) noexcept { m_attackHitRadius = radius; }
        std::string displayName() const override;
        float getHp() const { return m_hp; }
        float getMaxHp() const { return m_maxHp; }
        // Restores HP (clamped to [0, maxHp]); used by save/load.
        void setHp(float hp) { m_hp = hp < 0.f ? 0.f : (hp > m_maxHp ? m_maxHp : hp); }

        // ===== Construction =====
        // Puts the building into an under-construction state: workers advance the
        // progress toward buildTime, and most building features stay disabled until
        // it completes.
        void beginConstruction(float buildTimeSeconds, float startHp);
        // Advances construction by dt; returns true on the tick it completes.
        bool advanceConstruction(float dt);
        bool isComplete() const noexcept { return m_completed; }
        float buildProgress01() const noexcept;
        bool trainUnit(UnitType type);
        // Returns the cancelled unit type so callers can refund its cost, or nullopt
        // when the queue was empty.
        std::optional<UnitType> cancelLastTrain();
        void setUnitSpawnFn(UnitSpawnFn fn);
        int trainQueueSize() const { return static_cast<int>(m_trainQueue.size()); }
        UnitType trainQueueAt(int i) const noexcept;
        float trainProgress() const noexcept;

        void setRallyPoint(const Vector2D& point);
        Vector2D rallyPoint() const noexcept { return m_rallyPoint; }
        bool hasRallyPoint() const noexcept { return m_hasRallyPoint; }
        RuntimeState runtimeState() const;
        void restoreRuntimeState(const RuntimeState& state);

    private:
        // Training time for the front-of-queue unit, from its static data.
        float currentTrainTime() const;

        BuildingType m_type;
        Vector2D m_position;
        float m_maxHp;
        float m_hp;
        int m_teamId;
        float m_trainTime;
        float m_trainTimer = 0.f;
        std::deque<UnitType> m_trainQueue;
        UnitSpawnFn m_spawnFn;
        Vector2D m_rallyPoint{};
        bool m_hasRallyPoint = false;
        bool m_completed = true;
        float m_buildTime = 0.f;
        float m_buildProgress = 0.f;
        // Footprint half-diagonal in world units; covers every approach angle so a
        // unit blocked at any wall/corner still registers as in attack range.
        float m_attackHitRadius = 52.f;
        GameState m_state{};
    };

} // namespace rts::core::model
