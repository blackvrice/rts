#include <cmath>
#include <core/model/Unit.hpp>
#include <core/viewmodel/UnitViewModel.hpp>

namespace rts::core::model {
    Unit::Unit(manager::GameLogicManager& logic)
        : m_logic(logic)
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

    void Unit::updateMove(float dt) {
        if (m_action != ActionType::Move)
            return;

        Vector2D delta{
            m_moveTarget.x - m_position.x,
            m_moveTarget.y - m_position.y
        };

        float distSq = delta.x * delta.x + delta.y * delta.y;
        constexpr float ARRIVE_EPSILON = 0.5f;

        if (distSq <= ARRIVE_EPSILON * ARRIVE_EPSILON) {
            m_position = m_moveTarget;
            m_action = ActionType::Idle;
            return;
        }

        float dist = std::sqrt(distSq);
        Vector2D dir{ delta.x / dist, delta.y / dist };

        float moveDist = moveSpeed * dt;
        Vector2D step{ dir.x * moveDist, dir.y * moveDist };

        Vector2D nextPos{
            m_position.x + step.x,
            m_position.y + step.y
        };

        if (m_logic.canMoveUnitTo(*this, nextPos)) {
            m_position = nextPos;
        } else {
            Vector2D slideX{ m_position.x + step.x, m_position.y };
            Vector2D slideY{ m_position.x, m_position.y + step.y };

            bool moved = false;

            if (m_logic.canMoveUnitTo(*this, slideX)) {
                m_position = slideX;
                moved = true;
            }
            else if (m_logic.canMoveUnitTo(*this, slideY)) { // ✅ 수정
                m_position = slideY;
                moved = true;
            }

            if (!moved) {
                m_action = ActionType::Idle;
            }
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
    }

    Vector2D Unit::getPosition() const {
        return m_position;
    }

    void Unit::setPosition(const Vector2D& pos) {
        m_position = pos;
    }

    void Unit::holdPosition() {
        m_action = ActionType::Idle;
    }

    void Unit::stop() {
        m_action = ActionType::Idle;
    }

    const GameState& Unit::state() const {
        return m_state;
    }

    void Unit::buildRenderCommands(render::RenderQueue& out) const {
        // TODO: UnitRenderCommand 추가
    }

}
