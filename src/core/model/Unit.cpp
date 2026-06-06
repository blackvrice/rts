#include <cmath>
#include <iostream>
#include <algorithm>
#include <core/model/Unit.hpp>
#include <core/viewmodel/UnitViewModel.hpp>
#include <core/world/GridTransform.hpp>

namespace {
    float distanceSq(
        const rts::core::model::Vector2D& a,
        const rts::core::model::Vector2D& b) {
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        return dx * dx + dy * dy;
    }
}


namespace rts::core::model {
    Unit::Unit()
        : Unit(core::data::warriorUnitStaticData())
    {
    }

    Unit::Unit(const core::data::UnitStaticData& staticData)
    {
        // ===== 기본 스탯 초기화 =====
        m_position = { 0.f, 0.f };

        applyStaticData(staticData);
        attackTimer = 0.f;

        m_action = ActionType::Idle;
        m_animationAction = ActionType::Idle;
    }

    void Unit::applyStaticData(const core::data::UnitStaticData& staticData) {
        m_displayName = staticData.displayName;
        m_maxHp = staticData.maxHp;
        m_hp = m_maxHp;
        moveSpeed = staticData.moveSpeed;
        attackRange = staticData.attackRange;
        attackDamage = staticData.attackDamage;
        attackCooldown = staticData.attackCooldown;
        m_armor = staticData.armor;
    }

    ActionType Unit::getAction() const {
        return m_action;
    }

    ActionType Unit::getAnimationAction() const {
        return m_animationAction;
    }

    void Unit::moveTo(const Vector2D& target) {
        if (m_action == ActionType::Dead) return;
        m_action = ActionType::Move;
        m_animationAction = ActionType::Move;
        m_attackTarget = nullptr;
        m_moveTarget = target;
        m_finalTargetWorld = target;
        m_gridPath.clear();
    }

    void Unit::attack(IGameElement* target) {
        if (m_action == ActionType::Dead) return;
        if (!target) return;
        if (target->getAction() == ActionType::Dead) return;
        if (target->getTeamId() == m_teamId && m_teamId != TeamId::Neutral) return;

        m_action = ActionType::Attack;
        m_attackTarget = target;
        m_moveTarget = target->getPosition();
        m_finalTargetWorld = m_moveTarget;
        // The order is attack, but the sprite should run until the weapon is in range.
        m_animationAction = distanceSq(m_moveTarget, m_position) <= attackRange * attackRange
                                ? ActionType::Attack
                                : ActionType::Move;
        m_gridPath.clear();
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
            m_animationAction = ActionType::Idle;
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
            m_animationAction = ActionType::Idle;
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
            m_animationAction = ActionType::Idle;
            return;
        }

        Vector2D targetPos = m_attackTarget->getPosition();
        m_moveTarget = targetPos;
        m_finalTargetWorld = targetPos;

        float distSq = distanceSq(targetPos, m_position);
        float rangeSq = attackRange * attackRange;

        if (distSq > rangeSq) {
            m_animationAction = ActionType::Move;
            const float dist = std::sqrt(distSq);
            if (dist <= 0.0f) {
                return;
            }

            const Vector2D dir{(targetPos.x - m_position.x) / dist, (targetPos.y - m_position.y) / dist};
            float advance = moveSpeed * dt;
            const float maxAdvance = dist - attackRange;
            if (advance > maxAdvance) {
                advance = maxAdvance;
            }

            // Attack chase stops at weapon range instead of entering the target collision radius.
            if (advance > 0.0f) {
                m_position.x += dir.x * advance;
                m_position.y += dir.y * advance;
            }
            return;
        }

        m_animationAction = ActionType::Attack;
        attackTimer -= dt;
        if (attackTimer <= 0.f) {
            attackTimer = attackCooldown;
            m_attackTarget->takeDamage(attackDamage, this);
        }
    }

    void Unit::takeDamage(float amount, IGameElement* attacker) {
        if (m_action == ActionType::Dead)
            return;

        // Armor is static unit data and mitigates each hit while still allowing chip damage.
        const float mitigatedAmount = std::max(1.0f, amount - m_armor);
        m_hp -= mitigatedAmount;

        if (m_hp <= 0.f) {
            m_hp = 0.f;
            m_action = ActionType::Dead;
            m_animationAction = ActionType::Dead;
            m_attackTarget = nullptr;
            return;
        }

        if (attacker && m_action != ActionType::Attack && attacker->getTeamId() != m_teamId) {
            attack(attacker);
        }
    }

    float Unit::getHp() const {
        return m_hp;
    }

    float Unit::getMaxHp() const {
        return m_maxHp;
    }

    float Unit::getAttackDamage() const {
        return attackDamage;
    }

    float Unit::getAttackRange() const {
        return attackRange;
    }

    float Unit::getAttackCooldown() const {
        return attackCooldown;
    }

    float Unit::getMoveSpeed() const {
        return moveSpeed;
    }

    float Unit::getArmor() const {
        return m_armor;
    }

    std::string Unit::displayName() const {
        return m_displayName;
    }

    // ===== IElement / IGameElement 구현 =====

    void Unit::update() {
        // tick은 외부에서 호출
    }

    void Unit::idle() {
        if (m_action == ActionType::Dead) return;
        m_action = ActionType::Idle;
        m_animationAction = ActionType::Idle;
        m_attackTarget = nullptr;
        m_gridPath.clear();
    }

    Vector2D Unit::getPosition() const {
        return m_position;
    }

    void Unit::setPosition(const Vector2D& pos) {
        m_position = pos;
    }

    void Unit::holdPosition() {
        if (m_action == ActionType::Dead) return;
        m_action = ActionType::Hold;
        m_animationAction = ActionType::Hold;
        m_attackTarget = nullptr;
        m_gridPath.clear();
    }

    void Unit::setSelected(bool selected) {
        m_state.selected = selected;
    }

    int Unit::getTeamId() const {
        return m_teamId;
    }

    void Unit::setTeamId(int teamId) {
        m_teamId = teamId;
    }

    void Unit::setPath(path::Path p) {
        m_path = std::move(p);
        m_pathIndex = 0;
        // 상태 Move로 전환 등...
    }

    void Unit::setMoveTargetWithPath(const std::vector<path::GridPos>& gridPath,
                                 const Vector2D& finalWorldTarget)
    {
        if (m_action == ActionType::Dead) return;
        m_gridPath.clear();
        // PathManager returns the start cell too; movement should begin at the next cell.
        for (std::size_t i = 1; i < gridPath.size(); ++i) {
            m_gridPath.push_back(gridPath[i]);
        }

        m_finalTargetWorld = finalWorldTarget;
        m_moveTarget = finalWorldTarget;
        m_action = ActionType::Move;
        m_animationAction = ActionType::Move;
        m_attackTarget = nullptr;
    }

    const Vector2D& Unit::finalTargetWorld() const noexcept {
        return m_finalTargetWorld;
    }


    void Unit::stop() {
        if (m_action == ActionType::Dead) return;
        m_action = ActionType::Idle;
        m_animationAction = ActionType::Idle;
        m_attackTarget = nullptr;
        m_gridPath.clear();
    }

    const GameState& Unit::state() const {
        return m_state;
    }

}
