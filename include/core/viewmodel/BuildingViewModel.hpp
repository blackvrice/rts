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

        void update() override {}
        bool visible() const override { return m_visible; }
        void setVisible(bool v) override { m_visible = v; }
        const char* name() const override { return "Building"; }

    private:
        std::shared_ptr<model::Building> m_building;
        bool m_visible = true;
    };

}
