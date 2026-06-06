#pragma once
#include <memory>
#include "core/model/IViewModel.hpp"

namespace rts::core::model {
    class ResourceNode;
}

namespace rts::core::viewmodel {
    class ResourceNodeViewModel : public IViewModel {
    public:
        explicit ResourceNodeViewModel(std::shared_ptr<model::ResourceNode> node);

        void update() override {}
        bool visible()  const override { return m_visible; }
        void setVisible(bool v) override { m_visible = v; }
        const char* name() const override { return "ResourceNode"; }
        bool expired()  const override;
        const void* modelPtr() const override;
        void buildRenderCommands(render::RenderQueue& out) const override;

    private:
        std::shared_ptr<model::ResourceNode> m_node;
        bool m_visible = true;
    };
}
