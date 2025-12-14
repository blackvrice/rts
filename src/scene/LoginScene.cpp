//
// Created by black on 25. 12. 7..
//
#include "scene/LoginScene.hpp"
#include "scene/MenuScene.hpp"
#include <SFML/Window/Mouse.hpp>

#include "manager/UIManager.hpp"
#include "scene/SceneManager.hpp"
#include "ui/PasswordTextBox.hpp"

namespace rts::scene {

    LoginScene::LoginScene(const std::shared_ptr<SceneManager>& mgr, const std::shared_ptr<manager::GameManager> &game) : Scene(mgr, game), m_font("C:/Windows/Fonts/arial.ttf"), m_title(m_font)
    {
        m_ui = std::make_shared<rts::manager::UIManager>();
        // ID 입력
        m_nameBox = std::make_shared<rts::ui::TextBox>(
            sf::Vector2f(300, 300), sf::Vector2f(300, 40)
        );
        m_ui->add(m_nameBox);

        // Password 입력 박스
        m_passBox = std::make_shared<rts::ui::PasswordTextBox>(
            sf::Vector2f(300, 360), sf::Vector2f(300, 40)
        );
        m_ui->add(m_passBox);

        m_loginButton  = std::make_shared<rts::ui::Button>(
            sf::Vector2f(300, 420), sf::Vector2f(300, 40), "Login"
        );

        m_loginButton->onClick = [this]() {
            m_mgr->replace(std::make_shared<MenuScene>(m_mgr, m_game));
        };

        m_ui->add(m_loginButton);

        m_title.setCharacterSize(40);
        m_title.setString("Enter your nickname");
        m_title.setPosition({250, 200});
    }

    void LoginScene::handleEvent(const sf::Event& ev)
    {
        // 먼저 UI 이벤트
        if (m_ui->handleEvent(ev))
            return;

        // 버튼 클릭 체크
        if (ev.is<sf::Event::MouseButtonPressed>()) {
            auto m = ev.getIf<sf::Event::MouseButtonPressed>();

            if (m_loginButton->bounds().contains({(float)m->position.x, (float)m->position.y}))
            {
                // Login 성공 → MenuScene으로 이동
            }
        }

        // ESC 종료
        if (ev.is<sf::Event::KeyPressed>()) {
            auto k = ev.getIf<sf::Event::KeyPressed>();
            if (k->code == sf::Keyboard::Key::Escape)
                exit(0);
        }
    }

    void LoginScene::render(sf::RenderTarget& target)
    {
        target.clear(sf::Color(20,20,20));

        target.draw(m_title);

        m_ui->render(target);
    }



} // namespace rts::scene
