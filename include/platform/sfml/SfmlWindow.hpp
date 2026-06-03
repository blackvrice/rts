//
// Created by black on 25. 12. 22..
//

#pragma once

#include <platform/IWindow.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Shape.hpp>

#include "core/model/Key.hpp"

namespace rts::core::command {
    class UICommandBus;
}

namespace rts::platform::sfml {
    class SfmlWindow : public IWindow {
    public:
        explicit SfmlWindow(core::command::UICommandBus& bus);

        void clear() override;

        bool isOpen() const override;

        void pollEvents() override;

        void display() override;

        void* getNativeHandle() override {
            return &m_window;
        }

        void draw(const sf::Shape& renderTarget);
    private:
        static core::model::Key toKey(sf::Keyboard::Key k);
        static core::model::Scan toScan(sf::Keyboard::Scancode k);
        sf::RenderWindow m_window;
    };
}
