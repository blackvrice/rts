//
// Created by black on 25. 12. 25..
//
#include <game/login/LoginScene.hpp>

#include <game/login/LoginLogicManager.hpp>

namespace rts::scene {
    LoginScene::LoginScene(const std::shared_ptr<manager::IUIManager> &uiManager,
                           const std::shared_ptr<manager::ILogicManager> &logicManager) :
        IScene(uiManager, logicManager) {}

    void LoginScene::update() {
        m_uiManager->update();
    }

    void LoginScene::render() {
        m_uiManager->render();
    }
}
