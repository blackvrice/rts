//
// Created by black on 25. 12. 14..
//

#include "ui/TextBox.hpp"

namespace rts::ui {
    TextBox::TextBox(sf::Vector2f pos, sf::Vector2f size): m_font("C:/Windows/Fonts/arial.ttf"), m_text(m_font) {
        m_rect.setPosition(pos);
        m_rect.setSize(size);
        m_rect.setFillColor(sf::Color(30, 30, 30));
        m_rect.setOutlineColor(sf::Color::White);
        m_rect.setOutlineThickness(2.f);

        m_text.setCharacterSize(20);
        m_text.setPosition(pos + sf::Vector2f(5, 5));
    }

    std::string TextBox::value() const { return m_string; }

    bool TextBox::handleEvent(const sf::Event &ev) {
        // 클릭 시 Focus
        if (ev.is<sf::Event::MouseButtonPressed>()) {
            auto m = ev.getIf<sf::Event::MouseButtonPressed>();
            m_focused = bounds().contains({(float)m->position.x, (float)m->position.y});
        }

        // 입력 처리
        if (m_focused && ev.is<sf::Event::TextEntered>()) {
            auto t = ev.getIf<sf::Event::TextEntered>();
            if (t->unicode == 8) { // backspace
                if (!m_string.empty()) m_string.pop_back();
            }
            else if (t->unicode >= 32 && t->unicode <= 126) {
                m_string.push_back((char)t->unicode);
            }

            m_text.setString(m_string);
        }

        return m_focused;
    }

    void TextBox::render(sf::RenderTarget &target) {
        target.draw(m_rect);
        target.draw(m_text);
    }

    sf::FloatRect TextBox::bounds() const {
        return m_rect.getGlobalBounds();
    }
}
