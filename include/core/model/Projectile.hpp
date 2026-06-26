#pragma once
#include <functional>
#include <string>
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
                   data::SplashRadii splash = {}, SplashApplier applySplash = {},
                   std::string texturePath = {});

        void reset(Vector2D origin, IGameElement* target, float damage,
                   int ownerTeamId, float speed, data::WeaponType weaponType,
                   data::SplashRadii splash = {}, SplashApplier applySplash = {},
                   std::string texturePath = {});
        void tick(float dt);

        bool     expired()   const noexcept { return m_expired; }
        Vector2D position()  const noexcept { return m_position; }
        float    angleDeg()  const noexcept { return m_angleDeg; }
        int      ownerTeamId() const noexcept { return m_ownerTeamId; }
        data::WeaponType weaponType() const noexcept { return m_weaponType; }
        const std::string& texturePath() const noexcept { return m_texturePath; }

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
        std::string   m_texturePath;
        float         m_angleDeg { 0.f };
        bool          m_expired  { false };
    };
}
