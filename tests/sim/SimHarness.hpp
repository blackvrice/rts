#pragma once

// Headless simulation harness (roadmap M0-E4-01). Builds a GameWorld +
// GameLogicManager from a scenario map without SFML, feeds player commands
// straight into the logic router (the LogicThread normally does this), and
// advances fixed ticks. Tests then inspect world state under a read lock.

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "core/app/MatchConfig.hpp"
#include "core/command/CommandRouterBase.hpp"
#include "core/command/LogicCommand.hpp"
#include "core/command/LogicCommandBus.hpp"
#include "core/model/Building.hpp"
#include "core/model/IGameElement.hpp"
#include "core/model/PlayerResourceState.hpp"
#include "core/model/ResourceNode.hpp"
#include "core/model/Unit.hpp"
#include "core/sim/SimClock.hpp"
#include "core/world/GameWorld.hpp"
#include "game/game/GameLogicManager.hpp"

namespace rts::test {
    class SimHarness {
    public:
        // Passive enemy AI by default so scenarios only measure what the test
        // commands; replay recording is always off (tests never write files).
        explicit SimHarness(const std::string& mapPath, const bool enemyAiPassive = true) {
            core::app::MatchConfig config;
            config.mapPath = mapPath;
            config.enemyAiPassive = enemyAiPassive;
            config.recordReplay = false;
            m_logic = std::make_unique<core::manager::GameLogicManager>(
                m_bus, m_router, m_world, config);
        }

        core::world::GameWorld& world() { return m_world; }

        // Delivers a command synchronously, as LogicThread would between ticks.
        // Must be called without holding a world lock (see query helpers).
        void send(const core::command::LogicCommand& command) {
            m_router.dispatch(command);
        }

        void tick(const int count = 1) {
            for (int i = 0; i < count; ++i) {
                m_logic->tick(core::sim::kFixedDeltaSeconds);
            }
        }

        // Ticks until the predicate holds (checked under a read lock after each
        // tick) or maxTicks elapse. Returns true when the predicate held.
        bool tickUntil(const std::function<bool()>& predicate, const int maxTicks) {
            for (int i = 0; i < maxTicks; ++i) {
                tick();
                const auto lock = m_world.acquireReadLock();
                if (predicate()) {
                    return true;
                }
            }
            return false;
        }

        static int secondsToTicks(const float seconds) {
            return static_cast<int>(seconds * static_cast<float>(core::sim::kLogicTickHz));
        }

        void select(const core::ecs::EntityId id) {
            send(core::command::SelectEntityCommand(id.index, id.generation));
        }

        // Query helpers below need a world lock held by the caller. Never call
        // send()/select()/tick() while holding one: command handlers and ticks
        // take the world write lock themselves, so that would deadlock. Copy ids
        // and positions out of the locked scope first.
        template <typename T>
        std::vector<std::shared_ptr<T>> all(const std::function<bool(const T&)>& filter = {}) const {
            std::vector<std::shared_ptr<T>> result;
            for (const auto& element : m_world.getElements()) {
                auto typed = std::dynamic_pointer_cast<T>(element);
                if (!typed) {
                    continue;
                }
                if (const auto* game = dynamic_cast<const core::model::IGameElement*>(typed.get());
                    game && game->getAction() == core::model::ActionType::Dead) {
                    continue;
                }
                if (!filter || filter(*typed)) {
                    result.push_back(std::move(typed));
                }
            }
            return result;
        }

        std::vector<std::shared_ptr<core::model::Unit>> units(
            const int team, const ::rts::UnitType type) const {
            return all<core::model::Unit>([&](const core::model::Unit& unit) {
                return unit.getTeamId() == team && unit.unitType() == type;
            });
        }

        std::vector<std::shared_ptr<core::model::Building>> buildings(
            const int team, const core::model::BuildingType type) const {
            return all<core::model::Building>([&](const core::model::Building& building) {
                return building.getTeamId() == team && building.buildingType() == type;
            });
        }

        std::vector<std::shared_ptr<core::model::ResourceNode>> resources(
            const core::model::ResourceNode::ResourceType type) const {
            return all<core::model::ResourceNode>([&](const core::model::ResourceNode& node) {
                return node.type() == type;
            });
        }

        const core::model::PlayerResourceState& economy(const int team) const {
            return m_world.playerResources(team);
        }

    private:
        core::command::LogicCommandBus m_bus;
        core::command::LogicCommandRouter m_router;
        core::world::GameWorld m_world;
        std::unique_ptr<core::manager::GameLogicManager> m_logic;
    };
}
