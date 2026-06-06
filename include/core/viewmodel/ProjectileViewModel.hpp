#pragma once
#include <memory>
#include "core/model/IViewModel.hpp"
#include "core/model/Projectile.hpp"

namespace rts::core::viewmodel {

    class ProjectileViewModel : public IViewModel {
    public:
        explicit ProjectileViewModel(std::shared_ptr<model::Projectile> proj);

        bool expired() const override;
        const void* modelPtr() const override;
        void buildRenderCommands(render::RenderQueue& out) const override;

    private:
        std::shared_ptr<model::Projectile> m_proj;
    };

}
