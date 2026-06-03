//
// Created by black on 25. 12. 25..
//

#pragma once
#include <memory>
#include <unordered_map>

#include <core/manager/IUIManager.hpp>
#include <core/di/DIContainer.hpp>
#include <core/scene/IScene.hpp>

#include "core/command/LogicCommandBus.hpp"
#include "core/command/UICommandBus.hpp"
#include "game/game/GameScene.hpp"
#include "game/lobby/LobbyScene.hpp"
#include "game/login/LoginScene.hpp"

namespace rts::core::manager {
    enum class SceneId {
        Login,
        Game,
        Lobby,
    };
    class SceneManager {
    public:
        SceneManager(core::DIContainer& di, command::LogicCommandBus& logicBus, command::UICommandBus& uiBus) : di(di), m_logicBus(logicBus), m_uiBus(uiBus) {}

        void changeScene(SceneId id) {
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

        void update() const {
            m_scene->update();
        }

        void tick(float dt) const {
            m_scene->tick(dt);
        }

        void render() const {
            m_scene->render();
        }

        scene::IScene& getScene() const {
            return *m_scene;
        }

    private:
        std::shared_ptr<scene::IScene> m_scene;
        core::DIContainer& di;
        command::LogicCommandBus& m_logicBus;
        command::UICommandBus& m_uiBus;
    };
}
