#include <cmath>
#include <iostream>
#include <core/model/Unit.hpp>
#include <core/viewmodel/UnitViewModel.hpp>
#include <core/world/GridTransform.hpp>


namespace rts::core::model {
    Unit::Unit()
    {
        // ===== 기본 스탯 초기화 =====
        m_position = { 0.f, 0.f };

        m_maxHp = 100.f;
        m_hp    = m_maxHp;

        moveSpeed = 120.f;
        attackRange = 80.f;
        attackDamage = 10.f;
        attackCooldown = 0.8f;
        attackTimer = 0.f;

        m_action = ActionType::Idle;
    }
    ActionType Unit::getAction() const {
        return m_action;
    }

    void Unit::moveTo(const Vector2D& target) {
        m_action = ActionType::Move;
        m_moveTarget = target;
        m_finalTargetWorld = target;
        m_gridPath.clear();
    }

    void Unit::attack(IGameElement* target) {
        if (!target) return;
        m_action = ActionType::Attack;
        m_attackTarget = target;
    }

    void Unit::tick(float dt) {
        if (m_action == ActionType::Dead)
            return;

        switch (m_action) {
        case ActionType::Move:
            updateMove(dt);
            break;
        case ActionType::Attack:
            updateAttack(dt);
            break;
        default:
            break;
        }
    }

    void Unit::updateMove(float dt)
    {
        if (m_action != ActionType::Move) return;

        Vector2D delta{ m_moveTarget.x - m_position.x, m_moveTarget.y - m_position.y };
        float distSq = delta.x * delta.x + delta.y * delta.y;

        constexpr float ARRIVE_EPS = 1.0f; // 타일 중심으로 붙는 오차
        if (distSq <= ARRIVE_EPS * ARRIVE_EPS) {
            m_position = m_moveTarget;
            m_action = ActionType::Idle;
            return;
        }

        float dist = std::sqrt(distSq);
        Vector2D dir{ delta.x / dist, delta.y / dist };

        float step = moveSpeed * dt;
        if (step >= dist) {
            m_position = m_moveTarget;
        } else {
            m_position.x += dir.x * step;
            m_position.y += dir.y * step;
        }
    }

    void Unit::updateMove(float dt, const world::GridTransform& tf)
    {
        if (m_action != ActionType::Move) return;

        Vector2D target;

        if (!m_gridPath.empty()) {
            target = tf.gridToWorldCenter(m_gridPath.front());
        } else {
            target = m_finalTargetWorld;
        }

        Vector2D delta{ target.x - m_position.x, target.y - m_position.y };
        float distSq = delta.x*delta.x + delta.y*delta.y;

        constexpr float ARRIVE_EPS = 1.0f; // 타일 중심으로 붙는 오차
        if (distSq <= ARRIVE_EPS*ARRIVE_EPS) {
            if (!m_gridPath.empty()) {
                m_gridPath.pop_front();
                return; // 다음 노드로 계속
            }
            m_position = target;
            m_action = ActionType::Idle;
            return;
        }

        float dist = std::sqrt(distSq);
        Vector2D dir{ delta.x / dist, delta.y / dist };

        float step = moveSpeed * dt;
        if (step >= dist) {
            m_position = target;
        } else {
            m_position.x += dir.x * step;
            m_position.y += dir.y * step;
        }
    }



    void Unit::updateAttack(float dt) {
        if (m_action != ActionType::Attack)
            return;

        if (!m_attackTarget || m_attackTarget->getAction() == ActionType::Dead) {
            m_attackTarget = nullptr;
            m_action = ActionType::Idle;
            return;
        }

        Vector2D targetPos = m_attackTarget->getPosition();
        float dx = targetPos.x - m_position.x;
        float dy = targetPos.y - m_position.y;

        float distSq = dx * dx + dy * dy;
        float rangeSq = attackRange * attackRange;

        if (distSq > rangeSq) {
            m_moveTarget = targetPos;
            updateMove(dt);
            return;
        }

        attackTimer -= dt;
        if (attackTimer <= 0.f) {
            attackTimer = attackCooldown;
            m_attackTarget->takeDamage(attackDamage, this);
        }
    }

    void Unit::takeDamage(float amount, IGameElement* attacker) {
        if (m_action == ActionType::Dead)
            return;

        m_hp -= amount;

        if (m_hp <= 0.f) {
            m_hp = 0.f;
            m_action = ActionType::Dead;
            m_attackTarget = nullptr;
            return;
        }

        if (attacker && m_action != ActionType::Attack) {
            m_attackTarget = attacker;
            m_action = ActionType::Attack;
        }
    }

    float Unit::getHp() const {
        return m_hp;
    }

    float Unit::getMaxHp() const {
        return m_maxHp;
    }

    // ===== IElement / IGameElement 구현 =====

    void Unit::update() {
        // tick은 외부에서 호출
    }

    void Unit::idle() {
        m_action = ActionType::Idle;
        m_gridPath.clear();
    }

    Vector2D Unit::getPosition() const {
        return m_position;
    }

    void Unit::setPosition(const Vector2D& pos) {
        m_position = pos;
    }

    void Unit::holdPosition() {
        m_action = ActionType::Hold;
        m_gridPath.clear();
    }

    void Unit::setPath(path::Path p) {
        m_path = std::move(p);
        m_pathIndex = 0;
        // 상태 Move로 전환 등...
    }

    void Unit::setMoveTargetWithPath(const std::vector<path::GridPos>& gridPath,
                                 const Vector2D& finalWorldTarget)
    {
        m_gridPath.clear();
        // PathManager returns the start cell too; movement should begin at the next cell.
        for (std::size_t i = 1; i < gridPath.size(); ++i) {
            m_gridPath.push_back(gridPath[i]);
        }

        m_finalTargetWorld = finalWorldTarget;
        m_moveTarget = finalWorldTarget;
        m_action = ActionType::Move;
    }

    const Vector2D& Unit::finalTargetWorld() const noexcept {
        return m_finalTargetWorld;
    }


    void Unit::stop() {
        m_action = ActionType::Idle;
        m_gridPath.clear();
    }

    const GameState& Unit::state() const {
        return m_state;
    }

}
