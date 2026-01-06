//
// Created by black on 25. 12. 25..
//


#include "game/game/GameScene.hpp"

namespace rts::scene {
    GameScene::GameScene(const std::shared_ptr<manager::IUIManager> &uiManager,
                         const std::shared_ptr<manager::ILogicManager> &logicManager) : IScene(
        uiManager, logicManager) {
    }

    void GameScene::update() {
        m_logicManager->update();

        m_uiManager->update();
    }

    void GameScene::render() {
        m_uiManager->render();
    }
}
