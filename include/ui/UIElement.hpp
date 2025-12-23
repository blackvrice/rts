//
// Created by black on 25. 12. 7..
//
#pragma once
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/System/Vector2.hpp>

namespace rts::ui {

    class UIElement {
    public:
        virtual ~UIElement() = default;

        virtual void update() = 0;
        virtual bool isHovered() const = 0;

        virtual bool wantsTextCursor() const { return false; }
        virtual bool blocksWorldInput() const { return false; }

        virtual void render() = 0;
    };

}
