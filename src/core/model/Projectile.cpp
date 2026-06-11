#include "core/model/Projectile.hpp"
#include <cmath>
#include "core/model/IGameElement.hpp"

namespace rts::core::model {
    static constexpr float kPi = 3.14159265f;

    Projectile::Projectile(Vector2D origin, IGameElement* target, float damage,
                            int ownerTeamId, float speed, data::WeaponType weaponType,
                            data::SplashRadii splash, SplashApplier applySplash)
        : m_position(origin) {
        reset(origin, target, damage, ownerTeamId, speed, weaponType,
              splash, std::move(applySplash));
    }

    void Projectile::reset(Vector2D origin, IGameElement* target, float damage,
                           int ownerTeamId, float speed, data::WeaponType weaponType,
                           data::SplashRadii splash, SplashApplier applySplash) {
        m_position = origin;
        m_target = target;
        m_lastKnownTargetPos = target ? target->getPosition() : origin;
        m_damage = damage;
        m_ownerTeamId = ownerTeamId;
        m_speed = speed;
        m_weaponType = weaponType;
        m_splash = splash;
        m_applySplash = std::move(applySplash);
        m_expired = false;
        const float dx = m_lastKnownTargetPos.x - m_position.x;
        const float dy = m_lastKnownTargetPos.y - m_position.y;
        m_angleDeg = std::atan2(dy, dx) * (180.f / kPi);
    }

    void Projectile::tick(float dt) {
        if (m_expired) return;

        if (m_target && m_target->getAction() != ActionType::Dead) {
            m_lastKnownTargetPos = m_target->getPosition();
        } else {
            m_target = nullptr; // target died: fly to last known pos and fizzle
        }

        const float dx = m_lastKnownTargetPos.x - m_position.x;
        const float dy = m_lastKnownTargetPos.y - m_position.y;
        const float distSq = dx * dx + dy * dy;

        constexpr float kHitRadiusSq = 14.f * 14.f;
        if (distSq <= kHitRadiusSq) {
            if (m_splash.any() && m_applySplash) {
                // Area shot: damage everyone in range around the impact point.
                m_applySplash(m_lastKnownTargetPos, m_damage, m_weaponType,
                              m_ownerTeamId, m_splash);
            } else if (m_target && m_target->getAction() != ActionType::Dead) {
                m_target->takeDamage(
                    m_damage * data::damageMultiplier(m_weaponType, m_target->armorType()),
                    nullptr);
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
