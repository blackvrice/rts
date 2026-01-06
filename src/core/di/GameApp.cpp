//
// Created by black on 25. 12. 6..
//
// src/core/GameApp.cpp
#include "core/app/GameApp.hpp"

#include "core/command/LogicCommandBus.hpp"
#include "platform/sfml/SfmlRenderManager.hpp"
#include "platform/sfml/SfmlWindow.hpp"


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

        di.registerSingleton<render::RenderQueue>(
            [](auto &) { return std::make_shared<render::RenderQueue>(); }
        );

        di.registerSingleton<manager::SceneManager>(
            [](DIContainer &di) {
                return std::make_shared<manager::SceneManager>(di);
            }
        );

        di.registerSingleton<thread::LogicThread>(
            [](DIContainer &di) {
                auto &bus = *di.resolve<command::LogicCommandBus>();
                auto &router = *di.resolve<command::LogicCommandRouter>();
                return std::make_shared<thread::LogicThread>(bus, router);
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
                auto &logicBus = *di.resolve<command::LogicCommandBus>();
                auto &queue = *di.resolve<render::RenderQueue>();
                return std::make_shared<manager::GameUIManager>(router, logicBus, queue);
            }
        );
        di.registerScoped<manager::GameLogicManager>(
            [](DIContainer &di) {
                auto &bus = *di.resolve<command::LogicCommandBus>();
                auto &router = *di.resolve<command::LogicCommandRouter>();
                return std::make_shared<manager::GameLogicManager>(bus, router);
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
                auto &queue = *di.resolve<render::RenderQueue>();
                return std::make_shared<manager::LoginUIManager>(router, logicBus, queue);
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
            [](DIContainer &di) {
                return std::make_shared<scene::LobbyScene>(
                    di.resolve<manager::LoginUIManager>(),
                    di.resolve<manager::LoginLogicManager>()
                );
            }
        );

        // --- SceneManager ---
        di.registerSingleton<manager::SceneManager>(
            [](auto &di) {
                return std::make_shared<manager::SceneManager>(di);
            }
        );

        di.registerSingleton<render::IRenderManager>(
            [](DIContainer &di) {
                return std::make_shared<platform::sfml::SfmlRenderManager>();
            }
        );

        di.registerSingleton<platform::IWindow>(
            [](DIContainer &di) {
                auto &bus = *di.resolve<command::UICommandBus>();
                return std::static_pointer_cast<platform::IWindow>(std::make_shared<platform::sfml::SfmlWindow>(bus));
            }
        );

        di.registerSingleton<render::RenderContext>(
            [](DIContainer &di) {
                auto &window = *di.resolve<platform::IWindow>();
                return std::make_shared<render::RenderContext>(
                    window,
                    model::Vector2D(1920, 1080)
                );
            }
        );

        m_logicThread = di.resolve<thread::LogicThread>();
        m_uiBus = di.resolve<command::UICommandBus>();
        m_uiRouter = di.resolve<command::UICommandRouter>();
        m_logicBus = di.resolve<command::LogicCommandBus>();
        m_logicRouter = di.resolve<command::LogicCommandRouter>();
        m_sceneManager = di.resolve<manager::SceneManager>();
        m_renderQueue = di.resolve<render::RenderQueue>();
        m_renderContext = di.resolve<render::RenderContext>();
        m_renderManager = di.resolve<render::IRenderManager>();
        m_sceneManager->changeScene(manager::SceneId::Game);
    }

    void GameApp::run() const {
        using clock = std::chrono::steady_clock;
        auto& window = m_renderContext->window();
        while (window.isOpen()) {
            window.clear();
            auto start = clock::now();
            // 1️⃣ OS 이벤트 수집
            window.pollEvents();


            command::UICommandPtr cmd;
            while (m_uiBus->tryPop(cmd)) {
                m_uiRouter->dispatch(*cmd);
            }

            m_sceneManager->update();

            m_sceneManager->render();

            m_renderManager->execute(*m_renderQueue, *m_renderContext);

            window.display();
        }
    }
} // namespace rts::core
