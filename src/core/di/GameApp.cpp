//
// Created by black on 25. 12. 6..
//
// src/core/GameApp.cpp
#include "core/app/GameApp.hpp"

#include "core/data/DataPaths.hpp"
#include "core/data/DataRegistry.hpp"
#include "core/command/CommandRouterBase.hpp"
#include "core/command/LogicCommandBus.hpp"
#include "core/command/UICommandBus.hpp"
#include "core/command/AudioCommandBus.hpp"
#include "core/di/DIContainer.hpp"
#include "core/manager/CameraManager.hpp"
#include "game/game/GameLogicManager.hpp"
#include "game/game/GameScene.hpp"
#include "game/game/GameUIManager.hpp"
#include "game/login/LoginLogicManager.hpp"
#include "game/login/LoginScene.hpp"
#include "game/login/LoginUIManager.hpp"
#include "game/lobby/LobbyLogicManager.hpp"
#include "game/lobby/LobbyScene.hpp"
#include "game/lobby/LobbyUIManager.hpp"
#include "platform/sfml/SfmlRenderManager.hpp"
#include "platform/sfml/SfmlWindow.hpp"
#include "platform/sfml/SfmlAudioManager.hpp"
#include "core/command/AudioCommand.hpp"
#include "core/manager/IAudioManager.hpp"
#include "core/manager/SceneManager.hpp"
#include "core/manager/PathManager.hpp"
#include "core/model/Vector2D.hpp"
#include "core/render/IRenderManager.hpp"
#include "core/render/RenderContext.hpp"
#include "core/render/RenderQueue.hpp"
#include "core/thread/LogicThread.hpp"
#include "core/thread/AudioThread.hpp"
#include "core/world/GameWorld.hpp"
#include "core/world/GameWorldGridQuery.hpp"


namespace rts::core {
    GameApp::GameApp(DIContainer &di)
        : m_di(di) {
        // Load design-time data before any scene/unit/building is constructed so
        // every lookup sees the JSON-driven values (falls back to built-ins).
        data::DataRegistry::global().loadFromDirectory(data::DataRoot);

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

        di.registerSingleton<manager::CameraManager>(
            [](auto &) { return std::make_shared<manager::CameraManager>(); }
        );

        di.registerSingleton<thread::LogicThread>(
            [](DIContainer &di) {
                auto &bus = *di.resolve<command::LogicCommandBus>();
                auto &router = *di.resolve<command::LogicCommandRouter>();
                return std::make_shared<thread::LogicThread>(bus, router);
            }
        );

        di.registerSingleton<command::AudioCommandBus>(
            [](auto &) { return std::make_shared<command::AudioCommandBus>(); }
        );
        di.registerSingleton<command::AudioCommandRouter>(
            [](auto &) { return std::make_shared<command::AudioCommandRouter>(); }
        );
        di.registerSingleton<thread::AudioThread>(
            [](DIContainer &di) {
                auto &bus = *di.resolve<command::AudioCommandBus>();
                auto &router = *di.resolve<command::AudioCommandRouter>();
                return std::make_shared<thread::AudioThread>(bus, router);
            }
        );
        di.registerSingleton<manager::IAudioManager>(
            [](DIContainer &di) {
                auto &bus = *di.resolve<command::AudioCommandBus>();
                auto &router = *di.resolve<command::AudioCommandRouter>();
                return std::static_pointer_cast<manager::IAudioManager>(
                    std::make_shared<platform::sfml::SfmlAudioManager>(bus, router));
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
                auto &camera  = *di.resolve<manager::CameraManager>();
                return std::make_shared<manager::GameUIManager>(router, logicBus, queue, world, camera);
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

        // Lobby Scene (main menu)
        di.registerScoped<scene::LobbyScene>(
            [](DIContainer &di) {
                return std::make_shared<scene::LobbyScene>(
                    di.resolve<manager::LobbyUIManager>(),
                    di.resolve<manager::LobbyLogicManager>()
                );
            }
        );
        di.registerScoped<manager::LobbyUIManager>(
            [](DIContainer &di) {
                auto &router = *di.resolve<command::UICommandRouter>();
                auto &logicBus = *di.resolve<command::LogicCommandBus>();
                auto &queue = *di.resolve<render::RenderQueue>();
                auto &camera = *di.resolve<manager::CameraManager>();
                return std::make_shared<manager::LobbyUIManager>(router, logicBus, queue, camera);
            }
        );
        di.registerScoped<manager::LobbyLogicManager>(
            [](DIContainer &di) {
                auto &bus = *di.resolve<command::LogicCommandBus>();
                auto &router = *di.resolve<command::LogicCommandRouter>();
                return std::make_shared<manager::LobbyLogicManager>(bus, router);
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
                auto &logicThread = *di.resolve<thread::LogicThread>();
                return std::make_shared<manager::SceneManager>(di, bus, uiBus, logicThread);
            }
        );

        di.registerSingleton<render::IRenderManager>(
            [](DIContainer &di) {
                auto &uiBus = *di.resolve<command::UICommandBus>();
                auto &audioBus = *di.resolve<command::AudioCommandBus>();
                return std::make_shared<platform::sfml::SfmlRenderManager>(uiBus, audioBus);
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
                auto &camera = *di.resolve<manager::CameraManager>();
                return std::make_shared<render::RenderContext>(
                    window,
                    model::Vector2D(1920, 1080),
                    camera
                );
            }
        );

        m_logicThread = di.resolve<thread::LogicThread>();
        m_audioThread = di.resolve<thread::AudioThread>();
        m_audioBus = di.resolve<command::AudioCommandBus>();
        m_audioRouter = di.resolve<command::AudioCommandRouter>();
        m_audioManager = di.resolve<manager::IAudioManager>();
        // Install the audio manager into the thread before it starts; the thread
        // owns a reference and dispatches drained commands to its router handlers.
        m_audioBus->push(std::make_unique<command::ChangeAudioManagerCommand>(m_audioManager));
        m_uiBus = di.resolve<command::UICommandBus>();
        m_uiRouter = di.resolve<command::UICommandRouter>();
        m_logicBus = di.resolve<command::LogicCommandBus>();
        m_logicRouter = di.resolve<command::LogicCommandRouter>();
        m_sceneManager = di.resolve<manager::SceneManager>();
        m_renderQueue = di.resolve<render::RenderQueue>();
        m_renderContext = di.resolve<render::RenderContext>();
        m_renderManager = di.resolve<render::IRenderManager>();
        m_sceneManager->changeScene(manager::SceneId::Lobby);
    }

    void GameApp::run() const {
        using clock = std::chrono::steady_clock;
        auto& window = m_renderContext->window();
        m_logicThread->start();
        m_audioThread->start();
        while (window.isOpen()) {
            window.clear();
            // 1️⃣ OS 이벤트 수집
            window.pollEvents();


            command::UICommandPtr cmd;
            while (m_uiBus->tryPop(cmd)) {
                m_uiRouter->dispatch(*cmd);
            }

            // Apply any scene change requested this frame (after input dispatch, so the
            // UI router can be safely reset). Safe even while the logic thread runs.
            m_sceneManager->applyPendingScene();

            m_sceneManager->update();

            m_sceneManager->render();

            m_renderManager->execute(*m_renderQueue, *m_renderContext);

            window.display();
        }
        m_audioThread->stop();
        m_logicThread->stop();
    }
} // namespace rts::core
