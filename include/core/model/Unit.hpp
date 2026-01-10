//
// Created by black on 26. 1. 4..
//

#pragma once
#include <memory>

#include "IGameElement.hpp"
#include "IViewModel.hpp"
#include "UnitViewModel.hpp"

namespace rts::core::model {
    class Unit : public IGameElement, public std::enable_shared_from_this<Unit>{
    public:
        ActionType currentAction() const override;

        void moveTo(const Vector2D &target) override;

        void attack(IGameElement *target) override;

        void tick(float dt) override;

        void updateMove(float dt);

        void updateAttack(float dt);

        bool canMoveTo(const Vector2D &pos) const;

        void takeDamage(float amount, IGameElement *attacker) override;



        std::shared_ptr<IViewModel> buildViewModel() override;

    private:
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
    };
}
