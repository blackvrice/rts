//
// Created by black on 26. 1. 4..
//

#pragma once
#include <cstddef>
#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "IGameElement.hpp"
#include "core/data/UnitStaticData.hpp"
#include "core/path/GridTypes.hpp"

namespace rts::core::world {
    struct GridTransform;
}

namespace rts::core::model {
    class Unit : public IGameElement{
    public:
        explicit Unit();
        explicit Unit(const core::data::UnitStaticData& staticData);

        ActionType getAction() const override;
        ActionType getAnimationAction() const;

        void moveTo(const Vector2D &target) override;

        void attack(IGameElement *target) override;

        void tick(float dt) override;

        void updateMove(float dt);

        void updateMove(float dt, const world::GridTransform& tf);

        void updateAttack(float dt);

        void takeDamage(float amount, IGameElement *attacker) override;

        float getHp() const;

        float getMaxHp() const;
        float getAttackDamage() const;
        float getAttackRange() const;
        float getAttackCooldown() const;
        float getMoveSpeed() const;
        float getArmor() const;
        std::string displayName() const override;

        void update() override;
        const GameState& state() const override;
        Vector2D getPosition() const override;
        void setPosition(const Vector2D&) override;
        void idle() override;
        void stop() override;
        void holdPosition() override;
        void patrol(const Vector2D&, const Vector2D&) override {}
        void attackMove(const Vector2D&) override {}
        void gather(IGameElement*) override {}
        void build(int, const Vector2D&) override {}
        void cast(int, const Vector2D&) override {}
        void setSelected(bool selected) override;

        int getTeamId() const override;
        void setTeamId(int teamId) override;

        void setPath(path::Path p);
        void setMoveTargetWithPath(const std::vector<path::GridPos>& gridPath,
                                   const Vector2D& finalWorldTarget);
        const Vector2D& finalTargetWorld() const noexcept;

    private:
        void applyStaticData(const core::data::UnitStaticData& staticData);

        std::deque<path::GridPos> m_gridPath; // 다음 노드부터 pop_front
        Vector2D m_finalTargetWorld{};
        path::Path m_path;
        size_t m_pathIndex{0};

        ActionType m_action = ActionType::Idle;
        ActionType m_animationAction = ActionType::Idle;

        Vector2D m_position{};
        Vector2D m_moveTarget{};

        IGameElement* m_attackTarget = nullptr;

        std::string m_displayName { "Unit" };
        float moveSpeed = 120.f;

        float attackRange = 64.f;
        float attackCooldown = 0.8f;
        float attackTimer = 0.f;
        float attackDamage = 10.f;
        float m_armor = 0.f;

        float m_hp = 100.f;
        float m_maxHp = 100.f;

        int m_teamId{TeamId::Neutral};

        GameState m_state{};
    };
}
