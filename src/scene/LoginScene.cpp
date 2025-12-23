//
// Created by black on 25. 12. 7..
//
#include "scene/LoginScene.hpp"
#include "scene/MenuScene.hpp"
#include <SFML/Window/Mouse.hpp>

#include "manager/GameUIManager.hpp"
#include "../../include/manager/SceneManager.hpp"
#include "ui/PasswordTextBox.hpp"

namespace rts::scene {
    LoginScene::LoginScene(
    const std::shared_ptr<SceneManager>& mgr,
    const std::shared_ptr<manager::GameManager>& game,
    const std::shared_ptr<manager::CursorManager>& cursor
)
    : Scene(mgr, game)
    , m_ui(cursor)                     // ✅ CursorManager 주입
    , m_font("C:/Windows/Fonts/arial.ttf")
    , m_title(m_font)
    {
        // ID 입력
        std::shared_ptr<ui::TextBox> nameBox;
        nameBox = std::make_shared<ui::TextBox>(
            sf::Vector2f(300, 300),
            sf::Vector2f(300, 40)
        );
        m_ui->add(nameBox);

        // Password 입력
        m_passBox = std::make_shared<rts::ui::PasswordTextBox>(
            sf::Vector2f(300, 360),
            sf::Vector2f(300, 40)
        );
        m_ui.add(m_passBox);

        // Login 버튼
        m_loginButton = std::make_shared<rts::ui::Button>(
            sf::Vector2f(300, 420),
            sf::Vector2f(300, 40),
            "Login"
        );

        m_loginButton->onClick = [this]() {
            m_mgr->replace(std::make_shared<MenuScene>(m_mgr, m_game));
        };

        m_ui.add(m_loginButton);

        // 타이틀
        m_title.setCharacterSize(40);
        m_title.setString("Enter your nickname");
        m_title.setPosition({250.f, 200.f});
    }

    void LoginScene::handleEvent(const sf::Event& ev) {
        m_ui.handleEvent(ev);
    }

    void LoginScene::update(sf::RenderWindow& win) {
        m_ui.update(win);   // hover + cursor 결정
    }

    void LoginScene::render(sf::RenderTarget& target) {
        target.draw(m_title);
        m_ui.render(target);
    }
} // namespace rts::scene
