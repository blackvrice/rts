//
// Created by black on 25. 12. 25..
//

#pragma once

#include <memory>
#include <core/Manager/IUIManager.hpp>

#include "core/ui/IUIElement.hpp"
#include "core/ui/TextBox.hpp"


namespace rts::manager {
    class LoginUIManager : public IUIManager {
    public:
        explicit LoginUIManager(command::UICommandRouter&, command::LogicCommandBus&, core::render::RenderQueue&);

        void update() override;
        void render() override;

    private:
        std::vector<std::unique_ptr<core::ui::IUIElement>> m_elements;
        std::unique_ptr<core::ui::TextBox> m_id;
        std::unique_ptr<core::ui::TextBox> m_password;
    };
}
