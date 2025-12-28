//
// Created by black on 25. 12. 22..
//

#include <thread>
#include <platform/sfml/sfmlWindow.hpp>

#include "command/Command.hpp"
#include "command/LogicCommandBus.hpp"


namespace rts::platform::sfml {
    sfmlWindow::sfmlWindow(command::UICommandBus& bus)
        : IWindow(bus),
          m_window(sf::VideoMode({1920, 1080}), "RTS")
    {
    }

    bool sfmlWindow::isOpen() const {
        return m_window.isOpen();
    }

    void sfmlWindow::pollEvents() {
        while (auto ev = m_window.pollEvent()) {
            if (!ev.has_value())
                continue;

            const auto& event = ev.value();

            if (event.is<sf::Event::MouseButtonPressed>()) {
                const auto* mouse = event.getIf<sf::Event::MouseButtonPressed>();
                if (mouse->button == sf::Mouse::Button::Left) {
                    m_bus.push(
                        std::make_unique<command::MouseLeftPressedCommand>(mouse->position)
                    );
                } else if (mouse->button == sf::Mouse::Button::Right) {
                    m_bus.push(
                        std::make_unique<command::MouseRightPressedCommand>(mouse->position)
                    );
                }
            }

            if (event.is<sf::Event::MouseButtonReleased>()) {
                const auto* mouse = event.getIf<sf::Event::MouseButtonReleased>();
                if (mouse->button == sf::Mouse::Button::Left) {
                    m_bus.push(
                        std::make_unique<command::MouseLeftReleasedCommand>()
                    );
                } else if (mouse->button == sf::Mouse::Button::Right) {
                    m_bus.push(
                        std::make_unique<command::MouseRightReleasedCommand>()
                    );
                }
            }

            if (event.is<sf::Event::MouseMoved>()) {
                const auto* mouse = event.getIf<sf::Event::MouseMoved>();
                m_bus.push(
                    std::make_unique<command::MouseMoveCommand>(mouse->position)
                );
            }

            if (event.is<sf::Event::KeyPressed>()) {
                const auto* key = event.getIf<sf::Event::KeyPressed>();
                m_bus.push(
                    std::make_unique<command::KeyPressedCommand>(*key)
                );
            }
        }
    }


    void sfmlWindow::display() {
        m_window.display();
    }
}
