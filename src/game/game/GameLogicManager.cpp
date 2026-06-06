#include "game/game/GameLogicManager.hpp"

#include <limits>
#include <memory>

#include <core/model/Unit.hpp>
#include <core/model/UnitType.hpp>
#include <core/model/Building.hpp>
#include <core/model/PlayerResourceState.hpp>
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

            core::model::PlayerResourceState playerResources {};
            playerResources.gold = 0;
            playerResources.wood = 0;
            playerResources.foodUsed = 2;
            playerResources.foodCapacity = 10;
            playerResources.army = 2;
            m_world.setPlayerResources(core::model::TeamId::Player, playerResources);

            core::model::PlayerResourceState enemyResources {};
            enemyResources.gold = 0;
            enemyResources.wood = 0;
            enemyResources.foodUsed = 2;
            enemyResources.foodCapacity = 10;
            enemyResources.army = 2;
            m_world.setPlayerResources(core::model::TeamId::Enemy, enemyResources);
            
            // --- Player Units ---
            auto worker = std::make_shared<core::model::Unit>(::rts::UnitType::Worker);
            worker->setPosition({300.f, 240.f});
            worker->setTeamId(core::model::TeamId::Player);
            m_world.addElement(worker);

            auto unit3 = std::make_shared<core::model::Unit>();
            unit3->setPosition({360.f, 300.f});
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
                core::model::Vector2D{220.f, 220.f},
                core::model::TeamId::Player
            );
            m_world.addElement(townHall);

            auto enemyTownHall = std::make_shared<core::model::Building>(
                core::model::BuildingType::TownHall,
                core::model::Vector2D{760.f, 650.f},
                core::model::TeamId::Enemy
            );
            m_world.addElement(enemyTownHall);

            auto enemyBarracks = std::make_shared<core::model::Building>(
                core::model::BuildingType::Barracks,
                core::model::Vector2D{700.f, 560.f},
                core::model::TeamId::Enemy
            );
            m_world.addElement(enemyBarracks);

            // --- Resources ---
            auto goldMine = std::make_shared<core::model::ResourceNode>(
                core::model::Vector2D{120.f, 180.f},
                core::model::ResourceNode::ResourceType::Gold,
                5000
            );
            m_world.addElement(goldMine);

            auto woodForest = std::make_shared<core::model::ResourceNode>(
                core::model::Vector2D{120.f, 360.f},
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

        m_router.on<command::GatherCommand>([this](const command::GatherCommand &cmd) {
            handleGatherCommand(cmd);
        });

        m_router.on<command::StopCommand>([this](const command::StopCommand &) {
            auto lock = m_world.acquireWriteLock();
            for (auto &weak: m_selection.selected()) {
                if (auto element = weak.lock(); element && element->getAction() != model::ActionType::Dead) {
                    element->stop();
                }
            }
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
        applyReadyResourceDeliveries();
        handleGatherRedirects();
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
                candidate->getAction() == model::ActionType::Dead) {
                continue;
            }

            const bool alreadySelected = selectionContains(selected, candidate.get());
            const bool selectedResource =
                alreadySelected && std::dynamic_pointer_cast<model::ResourceNode>(candidate);
            // Drag-select can include resource nodes; workers still need the selected resource as a gather target.
            if (alreadySelected && !selectedResource) {
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

    std::shared_ptr<model::Building> GameLogicManager::findClosestDropOffFor(const model::Unit& worker) const {
        std::shared_ptr<model::Building> bestDropOff;
        float bestDistanceSq = std::numeric_limits<float>::max();

        for (const auto& element : m_world.getElements()) {
            auto building = std::dynamic_pointer_cast<model::Building>(element);
            if (!building ||
                !building->isDropOff() ||
                building->getAction() == model::ActionType::Dead ||
                building->getTeamId() != worker.getTeamId()) {
                continue;
            }

            const float candidateDistanceSq = distanceSq(
                building->getPosition(),
                worker.getPosition()
            );
            if (candidateDistanceSq < bestDistanceSq) {
                bestDistanceSq = candidateDistanceSq;
                bestDropOff = std::move(building);
            }
        }

        return bestDropOff;
    }

    void GameLogicManager::issueGatherToResource(model::ResourceNode& resource) {
        if (resource.isDepleted()) {
            return;
        }

        for (const auto& weak : m_selection.selected()) {
            auto element = weak.lock();
            auto worker = std::dynamic_pointer_cast<model::Unit>(element);
            if (!worker ||
                !worker->isWorker() ||
                worker->getAction() == model::ActionType::Dead) {
                continue;
            }

            auto dropOff = findClosestDropOffFor(*worker);
            if (!dropOff) {
                worker->stop();
                continue;
            }

            worker->gather(&resource, dropOff.get());
        }
    }

    std::shared_ptr<model::ResourceNode> GameLogicManager::findClosestAvailableResource(
        model::ResourceNode::ResourceType type, const model::Unit& requester) const
    {
        std::shared_ptr<model::ResourceNode> best;
        float bestDistSq = std::numeric_limits<float>::max();

        for (const auto& element : m_world.getElements()) {
            auto node = std::dynamic_pointer_cast<model::ResourceNode>(element);
            if (!node || node->isDepleted() || node->type() != type) {
                continue;
            }
            if (node->reservedGathererCount() >= node->maxGatherers()) {
                continue;
            }
            const float dSq = distanceSq(requester.getPosition(), node->getPosition());
            if (dSq < bestDistSq) {
                bestDistSq = dSq;
                best = node;
            }
        }
        return best;
    }

    void GameLogicManager::handleGatherRedirects() {
        for (const auto& element : m_world.getElements()) {
            auto worker = std::dynamic_pointer_cast<model::Unit>(element);
            if (!worker || !worker->isWorker() ||
                worker->getAction() == model::ActionType::Dead) {
                continue;
            }

            if (worker->isNeedingDropOffRedirect()) {
                auto newDropOff = findClosestDropOffFor(*worker);
                if (newDropOff) {
                    worker->redirectToDropOff(newDropOff.get());
                } else {
                    worker->stop();
                }
            } else if (worker->isNeedingResourceRedirect()) {
                auto newResource = findClosestAvailableResource(
                    worker->targetGatherType(), *worker);
                auto newDropOff = findClosestDropOffFor(*worker);
                if (newResource && newDropOff) {
                    worker->gather(newResource.get(), newDropOff.get());
                } else {
                    worker->stop();
                }
            }
        }
    }

    void GameLogicManager::applyReadyResourceDeliveries() {
        for (const auto& element : m_world.getElements()) {
            auto unit = std::dynamic_pointer_cast<model::Unit>(element);
            if (!unit) {
                continue;
            }

            const auto delivery = unit->takeReadyResourceDelivery();
            if (!delivery || delivery->amount <= 0) {
                continue;
            }

            auto resources = m_world.playerResources(unit->getTeamId());
            switch (delivery->type) {
                case model::ResourceNode::ResourceType::Gold:
                    resources.gold += delivery->amount;
                    break;
                case model::ResourceNode::ResourceType::Wood:
                    resources.wood += delivery->amount;
                    break;
            }

            m_world.setPlayerResources(unit->getTeamId(), resources);
        }
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

        if (auto resource = std::dynamic_pointer_cast<model::ResourceNode>(target)) {
            issueGatherToResource(*resource);
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

    void GameLogicManager::handleGatherCommand(const command::GatherCommand& cmd) {
        auto lock = m_world.acquireWriteLock();

        if (!cmd.hasWorldTarget()) {
            return;
        }

        const auto target = findCommandTargetAt(cmd.target(), m_selection.selected());
        auto resource = std::dynamic_pointer_cast<model::ResourceNode>(target);
        if (!resource) {
            return;
        }

        issueGatherToResource(*resource);
    }
}
