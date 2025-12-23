//
// Created by black on 25. 12. 7..
//
#pragma once
#include "manager/GameManager.hpp"
#include "scene/Scene.hpp"
#include "manager/GameUIManager.hpp"
#include "system/UISystem.hpp"
#include "ui/Button.hpp"
#include "ui/TextBox.hpp"
#include "ui/PasswordTextBox.hpp"


namespace rts::scene {

    // LoginScene.hpp
    class LoginScene : public Scene {
    public:
        LoginScene(
            const std::shared_ptr<SceneManager>& mgr,
            const std::shared_ptr<manager::GameManager>& game,
            const std::shared_ptr<manager::CursorManager>& cursor
        );

        void handleEvent(const sf::Event& ev) override;
        void update(sf::RenderWindow& win) override;
        void render(sf::RenderTarget& target) override;

    private:
        std::shared_ptr<system::UISystem> m_ui;   // ✅ Scene 소유
        sf::Font m_font;
        sf::Text m_title;

        std::shared_ptr<rts::ui::TextBox> m_nameBox;
        std::shared_ptr<rts::ui::PasswordTextBox> m_passBox;
        std::shared_ptr<rts::ui::Button> m_loginButton;
    };


} // namespace rts::scene
