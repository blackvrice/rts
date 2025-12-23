//
// Created by black on 25. 12. 6..
//
// include/rts/core/Command.hpp
#pragma once

#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>

#include "model/Element.hpp"

namespace rts::command {
    struct Command {
        virtual ~Command() = default;
    };

    struct SceneChangeCommand : Command {
        std::string sceneName;
    };

    struct PauseGameCommand : Command {};
    struct ResumeGameCommand : Command {};

    struct ToggleDebugDrawCommand : Command {};


    struct SelectCommand : Command {
        sf::FloatRect area;
        SelectCommand(sf::Vector2i first, sf::Vector2i second) {
            float minX = std::min(first.x, second.x);
            float sizeX = std::abs(second.x - first.x);
            float minY = std::min(first.y, second.y);
            float sizeY = std::abs(second.y - first.y);
            area = sf::FloatRect({minX, minY}, {sizeX, sizeY});
        }
    };

    struct SelectUnitCommand : Command {
        int unitId;
    };

    struct ClearSelectionCommand : Command {};

    struct MoveCommand : Command {
        int unitId;
        sf::Vector2f target;
    };

    struct StopCommand : Command {
        int unitId;
    };

    struct HoldPositionCommand : Command {
        int unitId;
    };

    struct AttackCommand : Command {
        int unitId;
        int targetId;
    };

    struct AttackMoveCommand : Command {
        int unitId;
        sf::Vector2f target;
    };

    struct PatrolCommand : Command {
        int unitId;
        sf::Vector2f from;
        sf::Vector2f to;
    };

    struct TrainUnitCommand : Command {
        int buildingId;
        int unitTypeId;
    };

    struct BuildCommand : Command {
        int buildingId;
        sf::Vector2f position;
    };

    struct CancelProductionCommand : Command {
        int buildingId;
    };

    struct GatherCommand : Command {
        int workerId;
        int resourceId;
    };

    struct ReturnResourceCommand : Command {
        int workerId;
    };

    struct UseAbilityCommand : Command {
        int casterId;
        int abilityId;
        sf::Vector2f target;
    };

    struct TickCommand : Command {
        uint32_t tick;
    };

    struct NetworkCommand : Command {
        uint32_t playerId;
        std::unique_ptr<Command> payload; // 내부 변환용
    };

    struct MouseLeftPressedCommand : Command {
        sf::Vector2i position;
        MouseLeftPressedCommand(sf::Vector2i position) {
            this->position = position;
        }
    };

    struct MouseRightPressedCommand : Command {
        sf::Vector2i position;
        MouseRightPressedCommand(sf::Vector2i position) {
            this->position = position;
        }
    };

    struct MouseLeftReleasedCommand : Command {};

    struct MouseRightReleasedCommand : Command {};

    struct MouseMoveCommand : Command {
        sf::Vector2i position;
        MouseMoveCommand(sf::Vector2i position) {
            this->position = position;
        }
    };

    struct KeyPressedCommand : Command {
        sf::Keyboard::Key code;
        sf::Keyboard::Scancode scancode;
        bool shift = false;
        bool alt = false;
        bool control = false;
        KeyPressedCommand(const sf::Event::KeyPressed* key_pressed) {
            this->code = key_pressed->code;
            this->shift = key_pressed->shift;
            this->alt = key_pressed->alt;
            this->control = key_pressed->control;
        }
    };

} // namespace rts::core