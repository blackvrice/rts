//
// Created by black on 25. 12. 7..
//


#include "ui/Button.hpp"

namespace rts::ui {
    Button::Button(sf::Vector2f pos, sf::Vector2f size, std::string text)
        : m_font("C:/Windows/Fonts/arial.ttf"), m_text(m_font)
    {
        m_rect.setPosition(pos);
        m_rect.setSize(size);
        m_rect.setFillColor(sf::Color(50, 50, 50));

        m_text.setString(text);
        m_text.setCharacterSize(16);

        // 1) 텍스트 크기 계산
        sf::FloatRect textBounds = m_text.getLocalBounds();

        // 2) 중심 정렬 계산
        float x = pos.x + (size.x / 2.f) - (textBounds.size.x / 2.f) - textBounds.position.x;
        float y = pos.y + (size.y / 2.f) - (textBounds.size.y / 2.f) - textBounds.position.y;

        // 3) 적용
        m_text.setPosition({x, y});
    }

    bool Button::handleEvent(const sf::Event &ev) {
        if (ev.is<sf::Event::MouseButtonPressed>()) {
            auto m = ev.getIf<sf::Event::MouseButtonPressed>();
            if (bounds().contains({static_cast<float>(m->position.x), static_cast<float>(m->position.y)})) {
                onClick();
                return true;
            }
        }
        return false;
    }

    sf::FloatRect Button::bounds() const {
        return m_rect.getGlobalBounds();
    }
}
