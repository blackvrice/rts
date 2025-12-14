//
// Created by black on 25. 12. 7..
//
#pragma once
#include "manager/GameManager.hpp"
#include "scene/Scene.hpp"
#include "manager/UIManager.hpp"
#include "ui/Button.hpp"
#include "ui/TextBox.hpp"

namespace rts::ui {
    class PasswordTextBox;
}

namespace rts::scene {

    class LoginScene : public Scene {
    public:
        LoginScene(const std::shared_ptr<SceneManager>& mgr,
            const std::shared_ptr<manager::GameManager> &game);

        void handleEvent(const sf::Event& ev) override;
        void update(float dt) override {}
        void render(sf::RenderTarget& target) override;

    private:
        std::shared_ptr<manager::UIManager> m_ui;
        std::shared_ptr<ui::TextBox> m_nameBox;
        std::shared_ptr<ui::PasswordTextBox> m_passBox;
        std::shared_ptr<ui::Button> m_loginButton;
        sf::Font m_font;
        sf::Text m_title;
    };

} // namespace rts::scene
