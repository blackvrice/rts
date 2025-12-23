//
// Created by black on 25. 12. 6..
//
// src/core/GameApp.cpp
#include "core/GameApp.hpp"

#include <memory>

#include "../../include/manager/SceneManager.hpp"
#include "scene/MenuScene.hpp"
#include "../../include/thread/LogicThread.hpp"
#include "manager/GameUIManager.hpp"

namespace rts::core {
    GameApp::GameApp(DIContainer &di)
        : m_di(di) {

        m_uiManager = std::make_shared<manager::GameUIManager>();
        m_uiBus = std::make_shared<command::CommandBus>();
        m_uiRouter = std::make_shared<command::CommandRouter>();
    }

    void GameApp::run() {
        using clock = std::chrono::steady_clock;
        while (m_window->isOpen()) {
            auto start = clock::now();
            // 1️⃣ OS 이벤트 수집
            m_window->pollEvents();

            command::CommandBus::CommandPtr cmd;
            while (m_uiBus->tryPop(cmd)) {
                m_uiRouter->dispatch(*cmd);
            }

            // 2️⃣ UI 업데이트 (UICommand 실행)
            m_uiManager->update();

            // 5️⃣ 렌더
            m_window->display();
        }
    }
} // namespace rts::core
