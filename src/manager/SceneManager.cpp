#include "manager/SceneManager.hpp"
#include <memory>

#include "scene/InGameScene.hpp"
#include "scene/MenuScene.hpp"

namespace rts::manager {
    void SceneManager::start() {
        // Scene 등록 (Factory 방식)
        m_sceneFactories["game"] = [] {
            return std::make_unique<scene::InGameScene>();
        };

        m_sceneFactories["menu"] = [] {
            return std::make_unique<scene::MenuScene>();
        };

        m_CommandRouter.on<core::SceneChangeCommand>(
        [this](const core::SceneChangeCommand& cmd) {
            requestSceneChange(cmd.sceneName);
        }
    );
    }
    void SceneManager::update() {
        if (!m_sceneChangeRequested)
            return;

        changeScene(m_nextScene);
        m_sceneChangeRequested = false;
    }
    void SceneManager::requestSceneChange(const std::string& name) {
        m_nextScene = name;
        m_sceneChangeRequested = true;
    }

    void SceneManager::changeScene(const std::string& name) {
        if (m_currentScene)
            m_currentScene->onExit();

        auto it = m_sceneFactories.find(name);
        if (it == m_sceneFactories.end())
            return; // 또는 assert

        m_currentScene = it->second();

        m_currentScene->onEnter();
    }

} // namespace rts::scene
