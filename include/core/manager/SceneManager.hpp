//
// Created by black on 25. 12. 25..
//

#pragma once
#include <memory>
#include <unordered_map>

#include <core/manager/ILogicManger.hpp>
#include <core/manager/IUIManager.hpp>
#include <core/di/DIContainer.hpp>
#include <core/scene/IScene.hpp>

#include "game/game/GameScene.hpp"
#include "game/lobby/LobbyScene.hpp"
#include "game/login/LoginScene.hpp"

namespace rts::manager {
    enum class SceneId {
        Login,
        Game,
        Lobby,
    };
    class SceneManager {
    public:
        SceneManager(core::DIContainer& di) : di(di) {}

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
        }

        void update() const {
            m_scene->update();
        }

        void render() const {
            m_scene->render();
        }

        scene::IScene& getScene() {
            return *m_scene;
        }

    private:
        std::shared_ptr<scene::IScene> m_scene;
        core::DIContainer& di;
    };
}
