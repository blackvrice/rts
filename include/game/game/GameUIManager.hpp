//
// Created by black on 25. 12. 25..
//

#pragma once

#include <core/Manager/IUIManager.hpp>

#include "core/model/IViewModel.hpp"
#include "core/ui/IUIElement.hpp"
#include "core/ui/SelectBox.hpp"

namespace rts::manager {
    class GameUIManager : public IUIManager {
    public:
        GameUIManager(command::UICommandRouter &router, command::LogicCommandBus &logicBus,
                      core::render::RenderQueue &render_queue);

        void update() override;

        void addViewModel(std::shared_ptr<core::model::IViewModel> vm);

        void render() override;
    private:
        std::vector<int> unitElement;
        std::vector<std::unique_ptr<core::ui::IUIElement>> m_elements;
        std::vector<std::shared_ptr<core::model::IViewModel>> m_viewModels;
    };
}
