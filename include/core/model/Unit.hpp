//
// Created by black on 26. 1. 4..
//

#pragma once
#include <memory>

#include "IGameElement.hpp"
namespace rts::manager {
    class GameLogicManager;
}
namespace rts::core::model {
    class Unit : public IGameElement, public std::enable_shared_from_this<Unit>{
    public:
        explicit Unit(manager::GameLogicManager& logic);

        ActionType getAction() const override;

        void moveTo(const Vector2D &target) override;

        void attack(IGameElement *target) override;

        void tick(float dt) override;

        void updateMove(float dt);

        void updateAttack(float dt);

        void takeDamage(float amount, IGameElement *attacker) override;

        float getHp() const;

        float getMaxHp() const;

        void update() override;
        void buildRenderCommands(render::RenderQueue&) const override;
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

    private:
        manager::GameLogicManager& m_logic;

        ActionType m_action = ActionType::Idle;

        Vector2D m_position{};
        Vector2D m_moveTarget{};

        IGameElement* m_attackTarget = nullptr;

        float moveSpeed = 120.f;

        float attackRange = 64.f;
        float attackCooldown = 0.8f;
        float attackTimer = 0.f;
        float attackDamage = 10.f;

        float m_hp = 100.f;
        float m_maxHp = 100.f;

        GameState m_state{};
    };
}
