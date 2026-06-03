#include "core/manager/SceneManager.hpp"

#include <memory>

#include "core/command/LogicCommand.hpp"
#include "core/command/LogicCommandBus.hpp"
#include "core/command/UICommand.hpp"
#include "core/command/UICommandBus.hpp"
#include "core/di/DIContainer.hpp"
#include "core/scene/IScene.hpp"
#include "game/game/GameScene.hpp"
#include "game/lobby/LobbyScene.hpp"
#include "game/login/LoginScene.hpp"

namespace rts::core::manager {
    SceneManager::SceneManager(core::DIContainer& di, command::LogicCommandBus& logicBus, command::UICommandBus& uiBus)
        : di(di)
        , m_logicBus(logicBus)
        , m_uiBus(uiBus) {
    }

    void SceneManager::changeScene(SceneId id) {
        if (m_scene != nullptr) di.endScope();
        di.beginScope();

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
