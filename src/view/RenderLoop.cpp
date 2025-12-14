//
// Created by black on 25. 12. 6..
//
// src/view/RenderLoop.cpp
#include "view/RenderLoop.hpp"

namespace rts::view {

    RenderLoop::RenderLoop(std::shared_ptr<scene::SceneManager> mgr)
        : m_mgr(mgr)
        , m_window(sf::VideoMode({1280, 720}), "RTS with Scenes")
    {
        m_window.setFramerateLimit(144);
    }

    void RenderLoop::run()
    {
        sf::Clock clock;
        while (m_window.isOpen()) {
            while (auto ev = m_window.pollEvent()) {
                if (ev.value().is<sf::Event::Closed>()) {
                    m_window.close();
                }

                if (m_mgr) {
                    m_mgr->handleEvent(ev.value());
                }
            }

            float dt = clock.restart().asSeconds();

            if (m_mgr) {
                m_mgr->update(dt);
            }

            m_window.clear();
            if (m_mgr) {
                m_mgr->render(m_window);
            }
            m_window.display();
        }
    }

} // namespace rts::view

