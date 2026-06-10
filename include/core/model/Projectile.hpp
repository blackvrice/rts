#pragma once
#include <functional>
#include "core/data/CombatTypes.hpp"
#include "core/model/Vector2D.hpp"

namespace rts::core::model {
    class IGameElement;

    // A traveling shot fired by a ranged attack. Homes toward its target's last
    // known position and applies weapon/armor-scaled damage on arrival. Shots
    // with a splash radius hand off to a GameWorld-injected area-damage callback.
    class Projectile {
    public:
        // Area-of-effect resolver: (impact center, base damage, weapon, owner team, radii).
        using SplashApplier = std::function<void(
            const Vector2D&, float, data::WeaponType, int, data::SplashRadii)>;

        Projectile(Vector2D origin, IGameElement* target, float damage,
                   int ownerTeamId, float speed, data::WeaponType weaponType,
                   data::SplashRadii splash = {}, SplashApplier applySplash = {});

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
        data::SplashRadii m_splash;
        SplashApplier m_applySplash;
        float         m_angleDeg { 0.f };
        bool          m_expired  { false };
    };
}
