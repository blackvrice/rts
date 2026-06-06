#pragma once
#include <deque>
#include <functional>
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
        using UnitSpawnFn = std::function<void(UnitType, const Vector2D&, int)>;
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
        std::string displayName() const override;
        float getHp() const { return m_hp; }
        float getMaxHp() const { return m_maxHp; }
        bool trainUnit(UnitType type);
        void cancelLastTrain();
        void setUnitSpawnFn(UnitSpawnFn fn);
        int trainQueueSize() const { return static_cast<int>(m_trainQueue.size()); }
        UnitType trainQueueAt(int i) const noexcept;
        float trainProgress() const noexcept;

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
        GameState m_state{};
    };

} // namespace rts::core::model
