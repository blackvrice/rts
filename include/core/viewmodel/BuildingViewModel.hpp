#pragma once
#include <memory>
#include "core/model/IViewModel.hpp"
#include "core/model/Building.hpp"

namespace rts::core::viewmodel {

    class BuildingViewModel : public IViewModel {
    public:
        explicit BuildingViewModel(std::shared_ptr<model::Building> building);

        bool expired() const override;
        const void* modelPtr() const override;
        void buildRenderCommands(render::RenderQueue& out) const override;

    private:
        std::shared_ptr<model::Building> m_building;
    };

}
