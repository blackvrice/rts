//
// Created by black on 25. 12. 25..
//

#pragma once
#include <cstdint>
#include <memory>

#include "core/manager/ILogicManager.hpp"
#include "game/game/systems/CollisionSystem.hpp"
#include "game/game/systems/ControlGroupSystem.hpp"
#include "game/game/systems/MovementSystem.hpp"
#include "game/game/systems/SelectionSystem.hpp"

namespace rts::core::model {
    class IGameElement;
    class Unit;
    struct Vector2D;
}

namespace rts::core::world {
    class GameWorld;
}

namespace rts::core::manager {
    class GameLogicManager final : public ILogicManager {
    public:
        GameLogicManager(command::LogicCommandBus &bus,
                         command::LogicCommandRouter &router,
                        core::world::GameWorld &world
                         );

        void update() override;

        void tick(float dt) override;

        bool canMoveUnitTo(const model::Unit &unit, const model::Vector2D &pos) const;


        void addSelectedElement(model::IGameElement &element);
        // Logic 전용 API
        void clearSelection();

        void selectElement(model::IGameElement &element);
        void handleMoveCommand(const command::MoveCommand& cmd);
        void handleAttackCommand(const command::AttackCommand& cmd);

    private:
        std::shared_ptr<model::IGameElement> findCommandTargetAt(
            const model::Vector2D& target,
            const SelectionSystem::SelectedList& selected) const;

        core::world::GameWorld& m_world;
        SelectionSystem m_selection;
        ControlGroupSystem m_controlGroups;
        CollisionSystem m_collision;
        MovementSystem m_movement;
    };
} // namespace rts::manager
