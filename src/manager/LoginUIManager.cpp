//
// Created by black on 25. 12. 25..
//
#include <game/login/LoginUIManager.hpp>
#include <core/ui/TextBox.hpp>

#include "core/command/CommandRouterBase.hpp"
#include "core/command/LogicCommandBus.hpp"
#include "core/manager/IUIManager.hpp"

namespace rts::manager {
    LoginUIManager::LoginUIManager(command::UICommandRouter& router, command::LogicCommandBus& logicBus) : manager::IUIManager(router, logicBus)  {
        m_elements.push_back(
            std::make_unique<ui::TextBox>(
                core::Vector2D(10, 10),
                core::Vector2D(10, 10)
            )
        );
    }


    void LoginUIManager::update() {
        for (const auto& element : m_elements) {
            element->update();
        }
    }

    void LoginUIManager::render() {
        for (const auto& element : m_elements) {
            element->render();
        }
    }

}
