//
// Created by black on 25. 12. 6..
//

#pragma once
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Event.hpp>
#include <manager/GameManager.hpp>

namespace rts::scene {

    class SceneManager; // 전방 선언

    class Scene {
    public:
        explicit Scene(const std::shared_ptr<SceneManager> &mgr,
            const std::shared_ptr<manager::GameManager> &m_game) : m_mgr(mgr), m_game(m_game) {}

        virtual ~Scene() = default;

        virtual void onEnter() {}
        virtual void onExit()  {}

        virtual void handleEvent(const sf::Event& ev) = 0;
        virtual void update(float dt) = 0;
        virtual void render(sf::RenderTarget& target) = 0;

    protected:
        std::shared_ptr<SceneManager> m_mgr;
        std::shared_ptr<rts::manager::GameManager> m_game;
    };

} // namespace rts::scene

