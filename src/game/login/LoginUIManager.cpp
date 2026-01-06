//
// Created by black on 25. 12. 25..
//
#include <game/login/LoginUIManager.hpp>
#include <core/ui/TextBox.hpp>

#include "core/command/CommandRouterBase.hpp"
#include "core/command/LogicCommandBus.hpp"
#include "core/font/FontManager.hpp"
#include "core/manager/IUIManager.hpp"

namespace rts::manager {
    LoginUIManager::LoginUIManager(command::UICommandRouter &router, command::LogicCommandBus &logicBus,
                                   core::render::RenderQueue &render_queue) : manager::IUIManager(
        router, logicBus, render_queue) {
        auto& fm = core::font::FontManager::instance();
        m_id = std::make_unique<core::ui::TextBox>(
            core::model::Vector2D(10, 10),
            core::model::Vector2D(40, 20),
            &fm.metrics()
        );

        m_id->setboardColor(0xFFFFFFFF);
        m_id->setBackgroundColor(0x000000FF);
        m_id->setTextColor(0xFFFFFFFF);
        m_password = std::make_unique<core::ui::TextBox>(
            core::model::Vector2D(10, 40),
            core::model::Vector2D(40, 20),
            &fm.metrics()
        );
        m_password->setboardColor(0xFFFFFFFF);
        m_password->setBackgroundColor(0x000000FF);
        m_password->setTextColor(0xFFFFFFFF);
        m_elements.push_back(std::move(m_id));
        m_elements.push_back(std::move(m_password));

        router.on<command::MouseLeftPressedCommand>(
            [this](const command::MouseLeftPressedCommand &cmd) {
                const core::model::Vector2D &pos = cmd.position();
                for (const auto &element: m_elements) {
                    element->MouseDown(pos);
                }
            }
        );

        router.on<command::KeyPressedCommand>(
            [this](const command::KeyPressedCommand &cmd) {
                for (const auto &element: m_elements) {
                    element->KeyDown({cmd.getCode(), cmd.getScanCode(), cmd.getMods()});
                }
            }
        );

        router.on<command::TextEnteredCommand>(
            [this](const command::TextEnteredCommand &cmd) {
                for (const auto &element: m_elements) {
                    element->TextInput(cmd.getChar());
                }
            }
        );
    }


    void LoginUIManager::update() {
        for (const auto &element: m_elements) {
            element->update();
        }
    }

    void LoginUIManager::render() {
        for (const auto &element: m_elements) {
            element->buildRenderCommands(m_renderQueue);
        }
    }
}
