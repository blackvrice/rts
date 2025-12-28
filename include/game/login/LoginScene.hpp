//
// Created by black on 25. 12. 25..
//
#pragma once

#include <core/scene/IScene.hpp>
#include <game/login/LoginLogicManager.hpp>
#include <game/login/LoginUIManager.hpp>

namespace rts::scene {
    class LoginScene : public IScene {
    public:
        LoginScene(const std::shared_ptr<manager::IUIManager> &uiManager, const std::shared_ptr<manager::ILogicManager> &logicManager);

        void update() override;
        void render() override;
    private:
        std::shared_ptr<manager::LoginUIManager> m_uiManager;
        std::shared_ptr<manager::LoginLogicManager> m_logicManager;
    };
}
