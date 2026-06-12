#include "core/viewmodel/ProjectileViewModel.hpp"
#include "core/model/Projectile.hpp"
#include "core/render/RenderCommand.hpp"
#include "core/render/RenderQueue.hpp"

namespace rts::core::viewmodel {
    ProjectileViewModel::ProjectileViewModel(std::shared_ptr<model::Projectile> proj)
        : m_proj(std::move(proj)) {}

    bool ProjectileViewModel::expired() const {
        return !m_proj || m_proj->expired();
    }

    const void* ProjectileViewModel::modelPtr() const {
        return m_proj.get();
    }

    void ProjectileViewModel::buildRenderCommands(render::RenderQueue& out) const {
        if (!m_proj || m_proj->expired()) return;

        const auto pos = m_proj->position();
        if (!m_proj->texturePath().empty()) {
            constexpr float kProjectileSpriteSize = 42.0f;
            out.emplace(
                core::render::RenderLayer::World,
                11,
                core::render::DrawSprite{
                    .x = pos.x - kProjectileSpriteSize * 0.5f,
                    .y = pos.y - kProjectileSpriteSize * 0.5f,
                    .w = kProjectileSpriteSize,
                    .h = kProjectileSpriteSize,
                    .texturePath = m_proj->texturePath(),
                    .frameCount = 1,
                    .framesPerSecond = 0.0f,
                    .trimTransparent = true,
                    .rotation = m_proj->angleDeg()
                });
            return;
        }

        // Fallback for ranged units that do not have authored projectile art yet.
        out.emplace(
            core::render::RenderLayer::World,
            11,
            core::render::DrawCircle{
                .cx = pos.x,
                .cy = pos.y,
                .radius = 5.0f,
                .border_color = 0xFF1A1A1Au,
                .color = 0xFFFFE066u  // warm bolt
            });
    }
}
