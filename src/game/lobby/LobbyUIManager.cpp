//
// Created by black on 26. 1. 1..
//

#include "game/lobby/LobbyUIManager.hpp"

#include "core/ui/TextBox.hpp"

namespace rts::manager {
    LobbyUIManager::LobbyUIManager(command::UICommandRouter &router, command::LogicCommandBus &logicBus,
                                   core::render::RenderQueue& render_queue) : manager::IUIManager(
        router, logicBus, render_queue) {
    }


    void LobbyUIManager::update() {
        for (const auto &element: m_elements) {
            element->update();
        }
    }

    void LobbyUIManager::render() {
        for (const auto &element: m_elements) {
            element->buildRenderCommands(m_renderQueue);
        }
    }
}
