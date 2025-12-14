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

        sf::FloatRect bounds() const override;

    private:
        bool m_focused = false;
        std::string m_actual;
        std::string m_masked;

        sf::RectangleShape m_rect;
        sf::Font m_font;
        sf::Text m_text;
    };

} // namespace rts::ui
