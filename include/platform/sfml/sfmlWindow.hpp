//
// Created by black on 25. 12. 22..
//

#pragma once

#include <platform/IWindow.hpp>
#include <SFML/Graphics/RenderWindow.hpp>

namespace rts::platform::sfml {
    class sfmlWindow : public IWindow {
    public:
        explicit sfmlWindow(command::UICommandBus& bus);

        bool isOpen() const override;

        void pollEvents() override;

        void display() override;
    private:
        sf::RenderWindow m_window;
    };
}
