#pragma once
#include "core/model/Vector2D.hpp"

namespace rts::core::model {
    class IGameElement;

    class Projectile {
    public:
        Projectile(Vector2D origin, IGameElement* target, float damage,
                   int ownerTeamId, float speed, int textureId);

        void tick(float dt);

        bool     expired()   const noexcept { return m_expired; }
        Vector2D position()  const noexcept { return m_position; }
        float    angleDeg()  const noexcept { return m_angleDeg; }
        int      textureId() const noexcept { return m_textureId; }

    private:
        Vector2D      m_position;
        IGameElement* m_target;
        Vector2D      m_lastKnownTargetPos;
        float         m_damage;
        int           m_ownerTeamId;
        float         m_speed;
        int           m_textureId;
        float         m_angleDeg { 0.f };
        bool          m_expired  { false };
    };
}
