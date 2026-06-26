#include "core/manager/SceneManager.hpp"

#include <memory>

#include <string>

#include "core/command/CommandRouterBase.hpp"
#include "core/command/LogicCommand.hpp"
#include "core/command/LogicCommandBus.hpp"
#include "core/command/UICommand.hpp"
#include "core/command/UICommandBus.hpp"
#include "core/di/DIContainer.hpp"
#include "core/scene/IScene.hpp"
#include "core/thread/LogicThread.hpp"
#include "game/game/GameScene.hpp"
#include "game/lobby/LobbyScene.hpp"
#include "game/login/LoginScene.hpp"

namespace rts::core::manager {
    namespace {
        int sceneIdFromName(const std::string& name) {
            if (name == "game")  return static_cast<int>(SceneId::Game);
            if (name == "lobby") return static_cast<int>(SceneId::Lobby);
            if (name == "login") return static_cast<int>(SceneId::Login);
            return -1;
        }
    }

    SceneManager::SceneManager(core::DIContainer& di, command::LogicCommandBus& logicBus,
                               command::UICommandBus& uiBus, thread::LogicThread& logicThread)
        : di(di)
        , m_logicBus(logicBus)
        , m_uiBus(uiBus)
        , m_logicThread(logicThread) {
        registerSceneChangeHandler();
    }

    void SceneManager::registerSceneChangeHandler() {
        // Scene changes are requested via SceneChangeCommand (e.g. lobby buttons).
        // The handler runs on the logic thread, so it only records the request; the
        // main loop applies it through applyPendingScene().
        auto& logicRouter = *di.resolve<command::LogicCommandRouter>();
        logicRouter.on<command::SceneChangeCommand>(
            [this](const command::SceneChangeCommand& cmd) {
                const int id = sceneIdFromName(cmd.sceneName());
                if (id >= 0) m_pendingScene.store(id, std::memory_order_relaxed);
            });
    }

    void SceneManager::applyPendingScene() {
        const int id = m_pendingScene.exchange(-1, std::memory_order_relaxed);
        if (id >= 0) {
            changeScene(static_cast<SceneId>(id));
        }
    }

    void SceneManager::changeScene(SceneId id) {
        // Pause the logic thread for the whole swap: clearing the shared routers,
        // tearing down the old scope (which destroys the outgoing logic manager), and
        // wiring the new scene must not race a command dispatch or a tick.
        auto swapLock = m_logicThread.acquireSwapLock();

        if (m_scene != nullptr) di.endScope();
        di.beginScope();

        // Drop the previous scene's UI handlers so they don't fire on the new scene
        // (the incoming UIManager re-registers during resolve below).
        di.resolve<command::UICommandRouter>()->clear();

        // Likewise drop every logic handler: the outgoing manager's lambdas capture a
        // raw `this` that is about to dangle, so they must not survive into the next
        // scene (a stale handler firing on a replayed command was a use-after-free
        // crash). Re-register the two persistent handlers the routers still need.
        di.resolve<command::LogicCommandRouter>()->clear();
        registerSceneChangeHandler();
        m_logicThread.registerRouterHandlers();

        switch (id) {
            case SceneId::Game:
                m_scene = di.resolve<scene::GameScene>();
                break;
            case SceneId::Login:
                m_scene = di.resolve<scene::LoginScene>();
                break;
            case SceneId::Lobby:
                m_scene = di.resolve<scene::LobbyScene>();
                break;
        }

        auto logic = m_scene->getLogicManager();
        m_logicBus.push(std::make_unique<command::ChangeLogicManagerCommand>(logic));
        m_uiBus.push(std::make_unique<command::ChangeUILogicSourceCommand>(logic));
    }

    void SceneManager::update() const {
        m_scene->update();
    }

    void SceneManager::tick(float dt) const {
        m_scene->tick(dt);
    }

    void SceneManager::render() const {
        m_scene->render();
    }

    scene::IScene& SceneManager::getScene() const {
        return *m_scene;
    }
}
