#include "core/model/Projectile.hpp"
#include <cmath>
#include <memory>
#include "core/model/IGameElement.hpp"

namespace rts::core::model {
    static constexpr float kPi = 3.14159265f;

    Projectile::Projectile(Vector2D origin, IGameElement* target, float damage,
                           int ownerTeamId, float speed, int textureId)
        : m_position(origin)
        , m_target(target)
        , m_lastKnownTargetPos(target ? target->getPosition() : origin)
        , m_damage(damage)
        , m_ownerTeamId(ownerTeamId)
        , m_speed(speed)
        , m_textureId(textureId) {
        const float dx = m_lastKnownTargetPos.x - m_position.x;
        const float dy = m_lastKnownTargetPos.y - m_position.y;
        m_angleDeg = std::atan2(dy, dx) * (180.f / kPi);
    }

    void Projectile::tick(float dt) {
        if (m_expired) return;

        if (m_target && m_target->getAction() != ActionType::Dead) {
            m_lastKnownTargetPos = m_target->getPosition();
        } else {
            m_target = nullptr; // target died: fly to last known pos
        }

        const float dx = m_lastKnownTargetPos.x - m_position.x;
        const float dy = m_lastKnownTargetPos.y - m_position.y;
        const float distSq = dx * dx + dy * dy;

        constexpr float kHitRadiusSq = 14.f * 14.f;
        if (distSq <= kHitRadiusSq) {
            if (m_target && m_target->getAction() != ActionType::Dead) {
                m_target->takeDamage(m_damage, nullptr);
            }
            m_expired = true;
            return;
        }

        const float dist = std::sqrt(distSq);
        const float step = m_speed * dt;
        const float move = step < dist ? step : dist;
        m_position.x += (dx / dist) * move;
        m_position.y += (dy / dist) * move;
        m_angleDeg = std::atan2(dy, dx) * (180.f / kPi);
    }
}
