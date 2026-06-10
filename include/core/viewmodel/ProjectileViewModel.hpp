#pragma once
#include <memory>
#include "core/model/IViewModel.hpp"
#include "core/model/Projectile.hpp"

namespace rts::core::viewmodel {

    class ProjectileViewModel : public IViewModel {
    public:
        explicit ProjectileViewModel(std::shared_ptr<model::Projectile> proj);

        void update() override {}  // projectiles are advanced by GameWorld, not the VM
        bool visible() const override { return m_visible; }
        void setVisible(bool v) override { m_visible = v; }
        const char* name() const override { return "Projectile"; }
        bool expired() const override;
        const void* modelPtr() const override;
        void buildRenderCommands(render::RenderQueue& out) const override;

    private:
        std::shared_ptr<model::Projectile> m_proj;
        bool m_visible { true };
    };

}
