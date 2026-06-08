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
        out.emplace(
            core::render::RenderLayer::World,
            11,
            core::render::DrawSprite{
                .x               = pos.x - 16.f,
                .y               = pos.y - 16.f,
                .w               = 32.f,
                .h               = 32.f,
                .texturePath     = m_proj->texturePath(),
                .sourceX         = 0,
                .sourceY         = 0,
                .sourceW         = 0,
                .sourceH         = 0,
                .frameCount      = 1,
                .framesPerSecond = 0.f,
                .trimTransparent = false,
                .showInHud       = false,
                .rotation        = m_proj->angleDeg()
            });
    }
}
