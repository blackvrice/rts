#include "ui/PasswordTextBox.hpp"
//
// Created by black on 25. 12. 7..
//
namespace rts::ui {

    PasswordTextBox::PasswordTextBox(sf::Vector2f pos, sf::Vector2f size): m_font("C:/Windows/Fonts/arial.ttf"), m_text(m_font) {
        m_rect.setPosition(pos);
        m_rect.setSize(size);
        m_rect.setFillColor(sf::Color(30, 30, 30));
        m_rect.setOutlineColor(sf::Color::White);
        m_rect.setOutlineThickness(2.f);

        m_text.setCharacterSize(20);
        m_text.setPosition(pos + sf::Vector2f(5, 5));
    }

    bool PasswordTextBox::handleEvent(const sf::Event &ev) {
        // Focus
        if (ev.is<sf::Event::MouseButtonPressed>()) {
            auto m = ev.getIf<sf::Event::MouseButtonPressed>();
            m_focused = bounds().contains({static_cast<float>(m->position.x), static_cast<float>(m->position.y)});
        }

        // Text input
        if (m_focused && ev.is<sf::Event::TextEntered>()) {
            auto t = ev.getIf<sf::Event::TextEntered>();

            // Backspace
            if (t->unicode == 8) {
                if (!m_actual.empty()) {
                    m_actual.pop_back();
                    m_masked.pop_back();
                }
            }
            // Visible ASCII
            else if (t->unicode >= 32 && t->unicode <= 126) {
                m_actual.push_back(static_cast<char>(t->unicode));
                m_masked.append("*"); // Masked char
            }

            m_text.setString(m_masked);
        }
        return m_focused;
    }

    void PasswordTextBox::render(sf::RenderTarget &target) {
        target.draw(m_rect);
        target.draw(m_text);
    }

    sf::FloatRect PasswordTextBox::bounds() const {
        return m_rect.getGlobalBounds();
    }
}