//
// Created by black on 25. 12. 25..
//


#include "game/lobby/LobbyScene.hpp"

#include "core/manager/ILogicManager.hpp"
#include "core/manager/IUIManager.hpp"

namespace rts::core::scene {
    LobbyScene::LobbyScene(const std::shared_ptr<manager::IUIManager> &uiManager,
                           const std::shared_ptr<manager::ILogicManager> &logicManager) : IScene(
        uiManager, logicManager) {
    }

    void LobbyScene::update() {
        m_uiManager->update();
    }

    void LobbyScene::render() {
        m_uiManager->render();
    }

    void LobbyScene::tick(float) {
        // No simulation in the lobby.
    }
}
