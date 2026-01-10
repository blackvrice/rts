//
// Created by black on 26. 1. 1..
//

#include "game/game/GameUIManager.hpp"

#include "core/ui/SelectBox.hpp"
#include "core/ui/TextBox.hpp"

namespace rts::manager {
    GameUIManager::GameUIManager(command::UICommandRouter &router, command::LogicCommandBus &logicBus,
                                 core::render::RenderQueue &render_queue) : manager::IUIManager(
        router, logicBus, render_queue) {
        router.on<command::MouseLeftPressedCommand>(
            [this](const command::MouseLeftPressedCommand &cmd) {
                const core::model::Vector2D &pos = cmd.position();
                for (const auto &element: m_elements) {
                    element->MouseDown(pos);
                }
            }
        );
        router.on<command::MouseMoveCommand>(
            [this](const command::MouseMoveCommand &cmd) {
                const core::model::Vector2D &pos = cmd.position();
                for (const auto &element: m_elements) {
                    element->MouseMove(pos);
                }
            }
        );
        router.on<command::MouseLeftReleasedCommand>(
            [this](const command::MouseLeftReleasedCommand &cmd) {
                const core::model::Vector2D &pos = cmd.position();
                for (const auto &element: m_elements) {
                    element->MouseUp(pos);
                }
            }
        );
        m_elements.push_back(std::make_unique<core::ui::SelectBox>(logicBus));
    }


    void GameUIManager::update() {
        for (auto it = m_viewModels.begin(); it != m_viewModels.end(); ) {
            auto& vm = *it;

            if (!vm || vm->expired()) {
                it = m_viewModels.erase(it);
                continue;
            }

            if (vm->visible()) {
                vm->update();
            }

            ++it;
        }
    }

    void GameUIManager::addViewModel(std::shared_ptr<core::model::IViewModel> vm) {
        m_viewModels.push_back(vm);
    }

    void GameUIManager::render() {
        m_renderQueue.clear();
        for (auto &element: m_elements) {
            element->buildRenderCommands(m_renderQueue);
        }
    }
}
