#pragma once

#include <memory>
#include <SFML/System.hpp>
#include <SFML/Window.hpp>

#include <core/command/Command.hpp>

#include "core/model/Event.hpp"
#include "core/model/Key.hpp"
#include "core/model/Vector2D.hpp"

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
        explicit MouseLeftPressedCommand(core::model::Vector2D position)
            : m_position(position) {
        }

        core::model::Vector2D position() const noexcept { return m_position; }

    private:
        core::model::Vector2D m_position;
    };

    class MouseRightPressedCommand final : public UICommand {
    public:
        explicit MouseRightPressedCommand(core::model::Vector2D position)
            : m_position(position) {
        }

        core::model::Vector2D position() const noexcept { return m_position; }

    private:
        core::model::Vector2D m_position;
    };

    class MouseLeftReleasedCommand final : public UICommand {
    public:
        MouseLeftReleasedCommand(core::model::Vector2D position)
            : m_position(position) {
        }

        core::model::Vector2D position() const noexcept { return m_position; }

    private:
        core::model::Vector2D m_position;
    };

    class MouseRightReleasedCommand final : public UICommand {
    public:
        MouseRightReleasedCommand(core::model::Vector2D position)
            : m_position(position) {
        }

        core::model::Vector2D position() const noexcept { return m_position; }

    private:
        core::model::Vector2D m_position;
    };

    class MouseMoveCommand final : public UICommand {
    public:
        explicit MouseMoveCommand(core::model::Vector2D position)
            : m_position(position) {
        }

        core::model::Vector2D position() const noexcept { return m_position; }

    private:
        core::model::Vector2D m_position;
    };

    // =========================================================
    // Keyboard
    // =========================================================
    class KeyPressedCommand final : public UICommand {
    public:
        explicit KeyPressedCommand(
            core::model::Key code,
            core::model::Scan scancode,
            core::model::KeyModifier mods
        ) : m_code(code), m_scancode(scancode), m_mods(mods) {
        }

        core::model::Key getCode() const { return m_code; }
        core::model::Scan getScanCode() const { return m_scancode; }
        core::model::KeyModifier getMods() const { return m_mods; }

    private:
        core::model::Key m_code;
        core::model::Scan m_scancode;
        core::model::KeyModifier m_mods;
    };

    class TextEnteredCommand final : public UICommand {
    public:
        explicit TextEnteredCommand(char32_t ch)
            : m_char(ch) {
        }

        char32_t getChar() const { return m_char; }

    private:
        char32_t m_char;
    };
} // namespace rts::command
