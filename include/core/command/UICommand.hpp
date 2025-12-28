#pragma once

#include <memory>
#include <SFML/System.hpp>
#include <SFML/Window.hpp>

#include <core/command/Command.hpp>

namespace rts::command {

    // =========================================================
    // Base UICommand
    // =========================================================
    class UICommand : public Command {
    public:
        virtual ~UICommand() = default;
    };

    using UICommandPtr = std::unique_ptr<UICommand>;

    // =========================================================
    // Mouse
    // =========================================================
    class MouseLeftPressedCommand final : public UICommand {
    public:
        explicit MouseLeftPressedCommand(sf::Vector2i position)
            : m_position(position) {}

        sf::Vector2i position() const noexcept { return m_position; }

    private:
        sf::Vector2i m_position;
    };

    class MouseRightPressedCommand final : public UICommand {
    public:
        explicit MouseRightPressedCommand(sf::Vector2i position)
            : m_position(position) {}

        sf::Vector2i position() const noexcept { return m_position; }

    private:
        sf::Vector2i m_position;
    };

    class MouseLeftReleasedCommand final : public UICommand {
    public:
        MouseLeftReleasedCommand() = default;
    };

    class MouseRightReleasedCommand final : public UICommand {
    public:
        MouseRightReleasedCommand() = default;
    };

    class MouseMoveCommand final : public UICommand {
    public:
        explicit MouseMoveCommand(sf::Vector2i position)
            : m_position(position) {}

        sf::Vector2i position() const noexcept { return m_position; }

    private:
        sf::Vector2i m_position;
    };

    // =========================================================
    // Keyboard
    // =========================================================
    class KeyPressedCommand final : public UICommand {
    public:
        explicit KeyPressedCommand(const sf::Event::KeyPressed& key)
            : m_code(key.code),
              m_scancode(key.scancode),
              m_shift(key.shift),
              m_alt(key.alt),
              m_control(key.control) {}

        sf::Keyboard::Key code() const noexcept { return m_code; }
        sf::Keyboard::Scancode scancode() const noexcept { return m_scancode; }
        bool shift() const noexcept { return m_shift; }
        bool alt() const noexcept { return m_alt; }
        bool control() const noexcept { return m_control; }

    private:
        sf::Keyboard::Key m_code;
        sf::Keyboard::Scancode m_scancode;
        bool m_shift;
        bool m_alt;
        bool m_control;
    };

} // namespace rts::command
