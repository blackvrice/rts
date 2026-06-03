//
// Created by black on 25. 12. 6..
//
// src/core/GameApp.cpp
#include "core/app/GameApp.hpp"

#include "core/command/LogicCommandBus.hpp"
#include "game/game/GameLogicManager.hpp"
#include "platform/sfml/SfmlRenderManager.hpp"
#include "platform/sfml/SfmlWindow.hpp"
#include "core/manager/PathManager.hpp"


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

        di.registerSingleton<thread::LogicThread>(
            [](DIContainer &di) {
                auto &bus = *di.resolve<command::LogicCommandBus>();
                auto &router = *di.resolve<command::LogicCommandRouter>();
                return std::make_shared<thread::LogicThread>(bus, router);
            }
        );

        di.registerScoped<world::GameWorld>(
            [](DIContainer& di) {
                return std::make_shared<world::GameWorld>();
            }
        );


        // --- Scenes (Scoped!) ---
        // Game Scene
        di.registerScoped<scene::GameScene>(
            [](DIContainer &di) {
                auto world = di.resolve<world::GameWorld>();
                return std::make_shared<scene::GameScene>(
                    di.resolve<manager::GameUIManager>(),
                    di.resolve<manager::GameLogicManager>(),
                    world
                );
            }
        );
        di.registerScoped<manager::GameUIManager>(
            [](DIContainer &di) {
                auto &router  = *di.resolve<command::UICommandRouter>();
                auto &logicBus= *di.resolve<command::LogicCommandBus>();
                auto &queue   = *di.resolve<render::RenderQueue>();
                auto &world   = *di.resolve<world::GameWorld>();     // ✅ 같은 스코프의 world
                return std::make_shared<manager::GameUIManager>(router, logicBus, queue, world);
            }
        );

        di.registerScoped<manager::GameLogicManager>(
            [](DIContainer &di) {
                auto &bus    = *di.resolve<command::LogicCommandBus>();
                auto &router = *di.resolve<command::LogicCommandRouter>();
                auto &world  = *di.resolve<world::GameWorld>();      // ✅ 같은 스코프의 world
                return std::make_shared<manager::GameLogicManager>(bus, router, world);
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

        // GridQuery: Scoped (World에 종속)
        di.registerScoped<world::GameWorldGridQuery>([](DIContainer& di){
            auto& world = *di.resolve<world::GameWorld>();
            return std::make_shared<world::GameWorldGridQuery>(world);
        });

        // PathManager: Singleton (Query를 멤버로 들지 않는 설계 권장)
        di.registerSingleton<manager::PathManager>([](DIContainer& di){
            return std::make_shared<manager::PathManager>();
        });


        // --- SceneManager ---
        di.registerSingleton<manager::SceneManager>(
            [](DIContainer &di) {
                auto &bus = *di.resolve<command::LogicCommandBus>();
                auto &uiBus =  *di.resolve<command::UICommandBus>();
                return std::make_shared<manager::SceneManager>(di, bus, uiBus);
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
        m_logicThread->start();
        while (window.isOpen()) {
            window.clear();
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
        m_logicThread->stop();
    }
} // namespace rts::core
