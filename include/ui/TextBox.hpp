//
// Created by black on 25. 12. 7..
//
#pragma once
#include "ui/UIElement.hpp"
#include <SFML/Graphics.hpp>

namespace rts::ui {

    class TextBox : public UIElement {
    public:
        TextBox(sf::Vector2f pos, sf::Vector2f size);

        std::string value() const;

        bool handleEvent(const sf::Event& ev) override;

        void render(sf::RenderTarget& target) override;

        bool wantsTextCursor() const override { return m_hovered; }
        bool blocksWorldInput() const override { return m_hovered; }

    private:
        bool m_focused = false;
        bool m_hovered = false;
        std::string m_string;

        sf::RectangleShape m_rect;
        sf::VertexArray line;
        sf::Font m_font;
        sf::Text m_text;

    };

} // namespace rts::ui
