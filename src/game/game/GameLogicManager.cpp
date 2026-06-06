#include "game/game/GameLogicManager.hpp"

#include <limits>
#include <memory>

#include <core/model/Unit.hpp>
#include <core/model/Building.hpp>
#include <core/model/ResourceNode.hpp>
#include <core/world/GameWorld.hpp>

namespace {
    constexpr float kAttackTargetPickRadius = 64.0f;
    constexpr float kAttackTargetPickRadiusSq = kAttackTargetPickRadius * kAttackTargetPickRadius;

    float distanceSq(
        const rts::core::model::Vector2D& a,
        const rts::core::model::Vector2D& b) {
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        return dx * dx + dy * dy;
    }

    bool selectionContains(
        const rts::core::manager::SelectionSystem::SelectedList& selected,
        const rts::core::model::IGameElement* candidate) {
        for (const auto& weak : selected) {
            if (auto element = weak.lock(); element.get() == candidate) {
                return true;
            }
        }

        return false;
    }

    bool isOpposingTeam(const int attackerTeamId, const int targetTeamId) {
        return attackerTeamId != rts::core::model::TeamId::Neutral &&
               targetTeamId != rts::core::model::TeamId::Neutral &&
               attackerTeamId != targetTeamId;
    }
}

namespace rts::core::manager {
    GameLogicManager::GameLogicManager(
        command::LogicCommandBus &bus,
        command::LogicCommandRouter &router,
        core::world::GameWorld &world)
        : m_world(world), ILogicManager(bus, router) {
        // Temporary debug units until spawning/production owns initial world population.
        {
            auto lock = m_world.acquireWriteLock();
            
            // --- Player Units ---
            auto unit = std::make_shared<core::model::Unit>();
            unit->setPosition({300.f, 300.f});
            unit->setTeamId(core::model::TeamId::Player);
            m_world.addElement(unit);

            auto unit3 = std::make_shared<core::model::Unit>();
            unit3->setPosition({350.f, 300.f});
            unit3->setTeamId(core::model::TeamId::Player);
            m_world.addElement(unit3);

            // --- Enemy Units ---
            auto unit2 = std::make_shared<core::model::Unit>();
            unit2->setPosition({600.f, 500.f});
            unit2->setTeamId(core::model::TeamId::Enemy);
            m_world.addElement(unit2);

            auto unit4 = std::make_shared<core::model::Unit>();
            unit4->setPosition({650.f, 500.f});
            unit4->setTeamId(core::model::TeamId::Enemy);
            m_world.addElement(unit4);

            // --- Buildings ---
            auto townHall = std::make_shared<core::model::Building>(
                core::model::BuildingType::TownHall,
                core::model::Vector2D{200.f, 200.f},
                core::model::TeamId::Player
            );
            m_world.addElement(townHall);

            auto enemyBarracks = std::make_shared<core::model::Building>(
                core::model::BuildingType::Barracks,
                core::model::Vector2D{700.f, 600.f},
                core::model::TeamId::Enemy
            );
            m_world.addElement(enemyBarracks);

            // --- Resources ---
            auto goldMine = std::make_shared<core::model::ResourceNode>(
                core::model::Vector2D{100.f, 100.f},
                core::model::ResourceNode::ResourceType::Gold,
                5000
            );
            m_world.addElement(goldMine);

            auto woodForest = std::make_shared<core::model::ResourceNode>(
                core::model::Vector2D{100.f, 400.f},
                core::model::ResourceNode::ResourceType::Wood,
                2000
            );
            m_world.addElement(woodForest);
        }

        m_router.on<command::SelectCommand>([this](const command::SelectCommand &cmd) {
            auto lock = m_world.acquireWriteLock();
            m_selection.selectInArea(m_world, cmd.area());
        });

        m_router.on<command::MoveCommand>([this](const command::MoveCommand &cmd) {
            handleMoveCommand(cmd);
        });

        m_router.on<command::AttackCommand>([this](const command::AttackCommand &cmd) {
            handleAttackCommand(cmd);
        });

        m_router.on<command::HoldPositionCommand>([this](const command::HoldPositionCommand &) {
            auto lock = m_world.acquireWriteLock();
            for (auto &weak: m_selection.selected()) {
                if (auto element = weak.lock(); element && element->getAction() != model::ActionType::Dead) {
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
            auto lock = m_world.acquireWriteLock();
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

    std::shared_ptr<model::IGameElement> GameLogicManager::findCommandTargetAt(
        const model::Vector2D& target,
        const SelectionSystem::SelectedList& selected) const {
        std::shared_ptr<model::IGameElement> bestTarget;
        float bestDistanceSq = std::numeric_limits<float>::max();

        for (const auto& element : m_world.getElements()) {
            auto candidate = std::dynamic_pointer_cast<model::IGameElement>(element);
            if (!candidate ||
                candidate->getAction() == model::ActionType::Dead ||
                selectionContains(selected, candidate.get())) {
                continue;
            }

            const float candidateDistanceSq = distanceSq(candidate->getPosition(), target);
            if (candidateDistanceSq <= kAttackTargetPickRadiusSq &&
                candidateDistanceSq < bestDistanceSq) {
                bestDistanceSq = candidateDistanceSq;
                bestTarget = std::move(candidate);
            }
        }

        return bestTarget;
    }

    void GameLogicManager::handleAttackCommand(const command::AttackCommand& cmd) {
        auto lock = m_world.acquireWriteLock();

        if (!cmd.hasWorldTarget()) {
            return;
        }

        int attackerTeam = model::TeamId::Neutral;
        for (const auto& weak : m_selection.selected()) {
            if (auto unit = weak.lock(); unit && unit->getAction() != model::ActionType::Dead) {
                attackerTeam = unit->getTeamId();
                break;
            }
        }

        const auto target = findCommandTargetAt(cmd.target(), m_selection.selected());
        if (!target) {
            m_movement.issueMove(m_world, m_selection.selected(), cmd.target());
            return;
        }

        // Right-click follows RTS convention: friendly target means move/approach,
        // opposing team target means attack.
        if (!isOpposingTeam(attackerTeam, target->getTeamId())) {
            m_movement.issueMove(m_world, m_selection.selected(), target->getPosition());
            return;
        }

        for (const auto& weak : m_selection.selected()) {
            if (auto attacker = weak.lock();
                attacker &&
                attacker.get() != target.get() &&
                attacker->getAction() != model::ActionType::Dead) {
                attacker->attack(target.get());
            }
        }
    }
}
