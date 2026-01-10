//
// Created by black on 26. 1. 6..
//
#include <cmath>
#include <core/model/Unit.hpp>

namespace rts::core::model {
    ActionType Unit::currentAction() const {
        return m_action;
    }


    void Unit::moveTo(const Vector2D &target) {
        m_action = ActionType::Move;
        m_moveTarget = target;
    }

    void Unit::attack(IGameElement *target) {
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

        // 목표 방향
        Vector2D delta {
            m_moveTarget.x - m_position.x,
            m_moveTarget.y - m_position.y
        };

        float distSq = delta.x * delta.x + delta.y * delta.y;

        // 도착 판정
        constexpr float ARRIVE_EPSILON = 0.5f;
        if (distSq <= ARRIVE_EPSILON * ARRIVE_EPSILON) {
            m_position = m_moveTarget;
            m_action = ActionType::Idle;
            return;
        }

        float dist = std::sqrt(distSq);

        // 이동 방향 (정규화)
        Vector2D dir {
            delta.x / dist,
            delta.y / dist
        };

        float moveDist = moveSpeed * dt;

        // 이번 프레임 이동 벡터
        Vector2D step {
            dir.x * moveDist,
            dir.y * moveDist
        };

        Vector2D nextPos {
            m_position.x + step.x,
            m_position.y + step.y
        };

        // 🔴 충돌 체크
        if (canMoveTo(nextPos)) {
            // 정상 이동
            m_position = nextPos;
        } else {
            // 🔥 슬라이딩 이동 (RTS에서 중요)
            Vector2D slideX { m_position.x + step.x, m_position.y };
            Vector2D slideY { m_position.x, m_position.y + step.y };

            bool moved = false;

            if (canMoveTo(slideX)) {
                m_position = slideX;
                moved = true;
            }
            else if (canMoveTo(slideY)) {
                m_position = slideY;
                moved = true;
            }

            // 완전히 막힌 경우 → 이동 중단
            if (!moved) {
                m_action = ActionType::Idle;
            }
        }
    }

    void rts::core::model::Unit::updateAttack(float dt) {
        if (m_action != ActionType::Attack)
            return;

        if (!m_attackTarget || m_attackTarget->currentAction() == ActionType::Dead) {
            m_attackTarget = nullptr;
            m_action = ActionType::Idle;
            return;
        }

        Vector2D targetPos = m_attackTarget->position();

        // 목표와의 거리 계산
        float dx = targetPos.x - m_position.x;
        float dy = targetPos.y - m_position.y;
        float distSq = dx * dx + dy * dy;

        float rangeSq = attackRange * attackRange;

        // 🔹 사거리 밖이면 → 추적 이동
        if (distSq > rangeSq) {
            m_moveTarget = targetPos;
            updateMove(dt);   // 직접 이동
            return;
        }

        // 🔹 사거리 안 → 공격
        attackTimer -= dt;

        if (attackTimer <= 0.f) {
            attackTimer = attackCooldown;

            // 실제 데미지 처리
            m_attackTarget->takeDamage(attackDamage, this);
        }
    }

    void Unit::takeDamage(float amount, IGameElement* attacker) {
        // 이미 죽었으면 무시
        if (m_action == ActionType::Dead)
            return;

        // 데미지 적용
        m_hp -= amount;

        // 사망 판정
        if (m_hp <= 0.f) {
            m_hp = 0.f;

            // 모든 행동 중단
            m_action = ActionType::Dead;
            m_attackTarget = nullptr;

            // 여기서는 즉시 제거하지 않음
            // → GameWorld에서 처리하는 게 정석
            return;
        }

        // 🔥 피격 반응 (Aggro)
        if (attacker && m_action != ActionType::Attack) {
            m_attackTarget = attacker;
            m_action = ActionType::Attack;
        }
    }

    bool Unit::canMoveTo(const Vector2D& pos) const {
        int tileX = static_cast<int>(pos.x) / 32;
        int tileY = static_cast<int>(pos.y) / 32;

        return !gameWorld->isBlocked(tileX, tileY);
    }

    std::shared_ptr<IViewModel> Unit::buildViewModel() {
        return std::make_shared<UnitViewModel>(shared_from_this());
    }
}