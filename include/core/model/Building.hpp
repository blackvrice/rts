#pragma once
#include <deque>
#include <functional>
#include <optional>
#include <string>
#include <memory>
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
        std::string displayName() const override;
        float getHp() const { return m_hp; }
        float getMaxHp() const { return m_maxHp; }
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

    private:
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
        GameState m_state{};
    };

} // namespace rts::core::model
