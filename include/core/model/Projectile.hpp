#pragma once
#include <string>

#include "core/model/Vector2D.hpp"

namespace rts::core::model {
    class IGameElement;

    class Projectile {
    public:
        // texturePath is relative to the asset root; empty draws nothing.
        Projectile(Vector2D origin, IGameElement* target, float damage,
                   int ownerTeamId, float speed, std::string texturePath);

        void tick(float dt);

        bool     expired()   const noexcept { return m_expired; }
        Vector2D position()  const noexcept { return m_position; }
        float    angleDeg()  const noexcept { return m_angleDeg; }
        const std::string& texturePath() const noexcept { return m_texturePath; }

    private:
        Vector2D      m_position;
        IGameElement* m_target;
        Vector2D      m_lastKnownTargetPos;
        float         m_damage;
        int           m_ownerTeamId;
        float         m_speed;
        std::string   m_texturePath;
        float         m_angleDeg { 0.f };
        bool          m_expired  { false };
    };
}
