#include "game/game/GameLogicManager.hpp"

#include <memory>

#include <core/model/Unit.hpp>
#include <core/world/GameWorld.hpp>

namespace rts::core::manager {
    GameLogicManager::GameLogicManager(
        command::LogicCommandBus &bus,
        command::LogicCommandRouter &router,
        core::world::GameWorld &world)
        : m_world(world), ILogicManager(bus, router) {
        // Temporary debug units until spawning/production owns initial world population.
        {
            auto lock = m_world.acquireWriteLock();
            auto unit = std::make_shared<core::model::Unit>();
            unit->setPosition({300.f, 300.f});

            m_world.addElement(unit);

            auto unit2 = std::make_shared<core::model::Unit>();
            unit2->setPosition({500.f, 500.f});

            m_world.addElement(unit2);
        }

        m_router.on<command::SelectCommand>([this](const command::SelectCommand &cmd) {
            auto lock = m_world.acquireReadLock();
            m_selection.selectInArea(m_world, cmd.area());
        });

        m_router.on<command::MoveCommand>([this](const command::MoveCommand &cmd) {
            handleMoveCommand(cmd);
        });

        m_router.on<command::AttackCommand>([this](const command::AttackCommand &) {
            // TODO
        });

        m_router.on<command::HoldPositionCommand>([this](const command::HoldPositionCommand &) {
            auto lock = m_world.acquireWriteLock();
            for (auto &weak: m_selection.selected()) {
                if (auto element = weak.lock()) {
                    element->holdPosition();
                }
            }
        });

        m_router.on<command::PatrolCommand>([this](const command::PatrolCommand &) {
            // TODO
        });

        m_router.on<command::ControlGroupAddCommand>([this](const auto &cmd) {
            m_controlGroups.add(cmd.groupId(), m_selection.selected());
        });

        m_router.on<command::ControlGroupAssignCommand>([this](const auto &cmd) {
            m_controlGroups.assign(cmd.groupId(), m_selection.selected());
        });

        m_router.on<command::ControlGroupSelectCommand>([this](const auto &cmd) {
            m_selection.replaceSelected(m_controlGroups.select(cmd.groupId()));
        });
    }

    void GameLogicManager::update() {
        // Logic-level update hook for future AI and state transitions.
    }

    void GameLogicManager::tick(float dt) {
        auto lock = m_world.acquireWriteLock();
        m_movement.update(m_world, dt, m_collision);
    }

    void GameLogicManager::selectElement(core::model::IGameElement &element) {
        m_selection.selectElement(element);
    }

    void GameLogicManager::addSelectedElement(core::model::IGameElement &element) {
        m_selection.addSelectedElement(element);
    }

    void GameLogicManager::clearSelection() {
        m_selection.clear();
    }

    bool GameLogicManager::canMoveUnitTo(
        const core::model::Unit &unit,
        const core::model::Vector2D &pos) const {
        return m_collision.canMoveUnitTo(m_world, unit, pos);
    }

    void GameLogicManager::handleMoveCommand(const command::MoveCommand& cmd) {
        auto lock = m_world.acquireWriteLock();
        m_movement.issueMove(m_world, m_selection.selected(), cmd.target());
    }
}
