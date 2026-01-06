//
// Created by black on 25. 12. 25..
//
#pragma once

#include <core/Manager/IUIManager.hpp>

#include "core/ui/IUIElement.hpp"

namespace rts::manager {
    class LobbyUIManager : public IUIManager {
    public:
        explicit LobbyUIManager(command::UICommandRouter&, command::LogicCommandBus&, core::render::RenderQueue&);

        void update() override;
        void render() override;

    private:
        std::vector<std::unique_ptr<core::ui::IUIElement>> m_elements;
    };
}
