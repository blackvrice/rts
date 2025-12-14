//
// Created by black on 25. 12. 7..
//

#include <SFML/Graphics.hpp>

#pragma once
namespace rts::ui {
    class UIElement {
    public:
        virtual ~UIElement() = default;

        virtual void update(float dt) {}
        virtual void render(sf::RenderTarget& target) = 0;

        virtual bool handleEvent(const sf::Event& ev) { return false; }

        virtual sf::FloatRect bounds() const = 0;
    };
}