//
// Created by black on 25. 12. 6..
//
// src/core/GameApp.cpp
#include "../../../include/core/app/GameApp.hpp"

#include "command/LogicCommandBus.hpp"
#include "platform/sfml/sfmlWindow.hpp"


namespace rts::core {
    GameApp::GameApp(DIContainer &di)
        : m_di(di) {
        di.registerSingleton<command::LogicCommandBus>(
            [](auto &) { return std::make_shared<command::LogicCommandBus>(); }
        );
        di.registerSingleton<command::LogicCommandRouter>(
            [](auto &) { return std::make_shared<command::LogicCommandRouter>(); }
        );

        di.registerSingleton<command::UICommandBus>(
            [](auto &) { return std::make_shared<command::UICommandBus>(); }
        );

        di.registerSingleton<command::UICommandRouter>(
            [](auto &) { return std::make_shared<command::UICommandRouter>(); }
        );

        di.registerSingleton<manager::SceneManager>(
            [](DIContainer &di) {
                return std::make_shared<manager::SceneManager>(di);
            }
        );


        // --- Scenes (Scoped!) ---
        // Game Scene
        di.registerScoped<scene::GameScene>(
            [](DIContainer &di) {
                return std::make_shared<scene::GameScene>(
                    di.resolve<manager::GameUIManager>(),
                    di.resolve<manager::GameLogicManager>()
                );
            }
        );
        di.registerScoped<manager::GameUIManager>(
            [](DIContainer &di) {
                auto &router = *di.resolve<command::UICommandRouter>();
                return std::make_shared<manager::GameUIManager>(router);
            }
        );
        di.registerScoped<manager::GameLogicManager>(
            [](DIContainer &di) {
                auto &router = *di.resolve<command::LogicCommandRouter>();
                return std::make_shared<manager::GameLogicManager>(router);
            }
        );

        // Login Scene
        di.registerScoped<scene::LoginScene>(
            [](DIContainer &di) {
                return std::make_shared<scene::LoginScene>(
                    di.resolve<manager::LoginUIManager>(),
                    di.resolve<manager::LoginLogicManager>()
                );
            }
        );
        di.registerScoped<manager::LoginUIManager>(
            [](DIContainer &di) {
                auto &router = *di.resolve<command::UICommandRouter>();
                auto &logicBus = *di.resolve<command::LogicCommandBus>();
                return std::make_shared<manager::LoginUIManager>(router, logicBus);
            }
        );
        di.registerScoped<manager::LoginLogicManager>(
            [](DIContainer &di) {
                auto &bus = *di.resolve<command::LogicCommandBus>();
                auto &router = *di.resolve<command::LogicCommandRouter>();
                return std::make_shared<manager::LoginLogicManager>(bus, router);
            }
        );


        di.registerScoped<scene::LobbyScene>(
            [](DIContainer &) {
                return std::make_shared<scene::LobbyScene>();
            }
        );

        // --- SceneManager ---
        di.registerSingleton<manager::SceneManager>(
            [](auto &di) {
                return std::make_shared<manager::SceneManager>(di);
            }
        );

        di.registerSingleton<platform::IWindow>(
            [](DIContainer &di) {
                auto &bus = *di.resolve<command::UICommandBus>();
                return std::static_pointer_cast<platform::IWindow>(std::make_shared<platform::sfml::sfmlWindow>(bus));
            }
        );

        m_logicThread = di.resolve<thread::LogicThread>();
        m_window = di.resolve<platform::IWindow>();
        m_uiBus = di.resolve<command::UICommandBus>();
        m_uiRouter = di.resolve<command::UICommandRouter>();
        m_sceneManager = di.resolve<manager::SceneManager>();
        m_sceneManager->changeScene(manager::SceneId::Login);
    }

    void GameApp::run() {
        using clock = std::chrono::steady_clock;
        while (m_window->isOpen()) {
            auto start = clock::now();
            // 1️⃣ OS 이벤트 수집
            m_window->pollEvents();


            command::UICommandPtr cmd;
            while (m_uiBus->tryPop(cmd)) {
                m_uiRouter->dispatch(*cmd);
            }

            m_sceneManager->update();

            m_sceneManager->render();

            m_window->display();
        }
    }
} // namespace rts::core
