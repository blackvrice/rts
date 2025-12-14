#include "scene/SceneManager.hpp"
#include <memory>

namespace rts::scene {

    void SceneManager::push(std::shared_ptr<Scene> scene){
        if (scene) {
            scene->onEnter();
            m_scenes.push_back(std::move(scene));
        }
    }

    void SceneManager::pop(){
        if (!m_scenes.empty()) {
            m_scenes.back()->onExit();
            m_scenes.pop_back();
        }
    }

    void SceneManager::replace(std::shared_ptr<Scene> scene){
        if (!m_scenes.empty()) {
            m_scenes.back()->onExit();
            m_scenes.pop_back();
        }
        push(std::move(scene));
    }

    void SceneManager::handleEvent(const sf::Event& ev) {
        if (!m_scenes.empty()) {
            m_scenes.back()->handleEvent(ev);
        }
    }

    void SceneManager::update(float dt) {
        if (!m_scenes.empty()) {
            m_scenes.back()->update(dt);
        }
    }

    void SceneManager::render(sf::RenderTarget& target) {
        if (!m_scenes.empty()) {
            m_scenes.back()->render(target);
        }
    }

} // namespace rts::scene
