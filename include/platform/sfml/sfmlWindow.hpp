//
// Created by black on 25. 12. 22..
//

#pragma once

#include <platform/IWindow.hpp>
#include <SFML/Graphics/RenderWindow.hpp>

namespace rts::platform::sfml {
    class sfmlWindow : public IWindow {
        sf::RenderWindow m_window;

    public:
        sfmlWindow(const std::shared_ptr<command::CommandBus> &bus,  const std::shared_ptr<command::CommandRouter> &router);

        bool isOpen() const override;

        void pollEvents() override;

        void display() override;
    };
}
