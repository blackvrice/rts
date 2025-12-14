//
// Created by black on 25. 12. 6..
//
#include "scene/MenuScene.hpp"
#include "scene/InGameScene.hpp"
#include "scene/SceneManager.hpp"

namespace rts::scene {

    MenuScene::MenuScene(const std::shared_ptr<SceneManager> &mgr,
                const std::shared_ptr<manager::GameManager> &game)
        : Scene(mgr, game), m_font("C:/Windows/Fonts/arial.ttf"),
          m_title(m_font)
    {
        m_title.setString("PRESS ENTER TO PLAY");
        m_title.setFillColor(sf::Color::White);
        m_title.setCharacterSize(42);
        m_title.setPosition({200.f, 300.f});
    }

    void MenuScene::handleEvent(const sf::Event& ev)
    {
        if (ev.is<sf::Event::KeyPressed>()) {
            auto key = ev.getIf<sf::Event::KeyPressed>();
            if (key && key->code == sf::Keyboard::Key::Enter) {

                // 🚀 GameManager 를 가진 InGameScene 생성
                auto newScene = std::make_shared<InGameScene>(m_mgr, m_game);

                m_mgr->replace(newScene);
            }
        }
    }

    void MenuScene::render(sf::RenderTarget& target)
    {
        target.clear(sf::Color::Black);
        target.draw(m_title);
    }

} // namespace rts::scene
