#pragma once
#include "core/data/CombatTypes.hpp"
#include "core/model/Vector2D.hpp"

namespace rts::core::model {
    class IGameElement;

    // A traveling shot fired by a ranged attack. Homes toward its target's last
    // known position and applies weapon/armor-scaled damage on arrival.
    class Projectile {
    public:
        Projectile(Vector2D origin, IGameElement* target, float damage,
                   int ownerTeamId, float speed, data::WeaponType weaponType);

        void tick(float dt);

        bool     expired()   const noexcept { return m_expired; }
        Vector2D position()  const noexcept { return m_position; }
        float    angleDeg()  const noexcept { return m_angleDeg; }
        int      ownerTeamId() const noexcept { return m_ownerTeamId; }

    private:
        Vector2D      m_position;
        IGameElement* m_target;
        Vector2D      m_lastKnownTargetPos;
        float         m_damage;
        int           m_ownerTeamId;
        float         m_speed;
        data::WeaponType m_weaponType;
        float         m_angleDeg { 0.f };
        bool          m_expired  { false };
    };
}
