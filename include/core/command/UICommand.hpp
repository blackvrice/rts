#pragma once

#include <cstdint>
#include <memory>
#include <SFML/System.hpp>
#include <SFML/Window.hpp>

#include <core/command/Command.hpp>

#include "core/model/Event.hpp"
#include "core/model/Key.hpp"
#include "core/model/Vector2D.hpp"

namespace rts::core::manager {
    class ILogicManager;
}

namespace rts::core::command {
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

    class KeyReleasedCommand final : public UICommand {
    public:
        explicit KeyReleasedCommand(
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

    // =========================================================
    // Gameplay Intent
    // =========================================================
    enum class GameplayInputAction : std::uint8_t {
        Move,
        Stop,
        HoldPosition,
        Attack,
        AttackMove,
        Patrol,
        TrainUnit,
        Build,
        CancelProduction,
        Gather,
        ReturnResource,
        Repair,
        UseAbility
    };

    class GameplayInputCommand final : public UICommand {
    public:
        explicit GameplayInputCommand(GameplayInputAction action)
            : m_action(action) {
        }

        GameplayInputAction action() const noexcept { return m_action; }

    private:
        GameplayInputAction m_action;
    };

    // A click on the minimap, in normalized 0..1 world coordinates. The left button
    // recenters the camera; the right button issues a world order at that point.
    class MinimapCommand final : public UICommand {
    public:
        MinimapCommand(float u, float v, bool rightButton)
            : m_u(u), m_v(v), m_right(rightButton) {}

        float u() const noexcept { return m_u; }
        float v() const noexcept { return m_v; }
        bool isRight() const noexcept { return m_right; }

    private:
        float m_u;
        float m_v;
        bool m_right;
    };

    // Chosen from a worker's build submenu: arm placement of this building type.
    class BuildMenuSelectCommand final : public UICommand {
    public:
        explicit BuildMenuSelectCommand(int buildingTypeId)
            : m_buildingTypeId(buildingTypeId) {}

        int buildingTypeId() const noexcept { return m_buildingTypeId; }

    private:
        int m_buildingTypeId;
    };

    // Clicking a portrait in the multi-selection list selects that single entity.
    class SelectEntityUICommand final : public UICommand {
    public:
        SelectEntityUICommand(std::uint32_t index, std::uint32_t generation)
            : m_index(index), m_generation(generation) {}

        std::uint32_t index() const noexcept { return m_index; }
        std::uint32_t generation() const noexcept { return m_generation; }

    private:
        std::uint32_t m_index;
        std::uint32_t m_generation;
    };

    // Chosen from a production building's command card: train this unit type.
    class TrainMenuSelectCommand final : public UICommand {
    public:
        explicit TrainMenuSelectCommand(int unitTypeId)
            : m_unitTypeId(unitTypeId) {}

        int unitTypeId() const noexcept { return m_unitTypeId; }

    private:
        int m_unitTypeId;
    };

    class ChangeUILogicSourceCommand final : public UICommand {
    public:
        explicit ChangeUILogicSourceCommand(
            std::shared_ptr<manager::ILogicManager> logicManager)
            : m_logicManager(std::move(logicManager)) {}

        const std::shared_ptr<manager::ILogicManager>& logic() const {
            return m_logicManager;
        }

    private:
        std::shared_ptr<manager::ILogicManager> m_logicManager;
    };
} // namespace rts::command
