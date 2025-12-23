//
// Created by black on 25. 12. 7..
//
#pragma once
#include "ui/UIElement.hpp"
#include <SFML/Graphics.hpp>

namespace rts::ui {
    class PasswordTextBox : public UIElement {
    public:
        PasswordTextBox(sf::Vector2f pos, sf::Vector2f size);

        std::string value() const { return m_actual; }

        bool handleEvent(const sf::Event& ev) override;

        void render(sf::RenderTarget& target) override;

        bool wantsTextCursor() const override { return m_hovered; }
        bool blocksWorldInput() const override { return m_hovered; }

    private:
        bool m_hovered = false;
        bool m_focused = false;
        std::string m_actual;
        std::string m_masked;

        sf::RectangleShape m_rect;
        sf::Font m_font;
        sf::Text m_text;
    };

} // namespace rts::ui
