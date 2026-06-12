#pragma once

#include <memory>
#include <string>
#include <cstdint>



#include "Command.hpp"
#include "core/ecs/EntityId.hpp"
#include "core/model/Rect.hpp"
#include "core/model/Vector2D.hpp"

namespace rts::core::manager {
    class ILogicManager;
}

namespace rts::core::command {
    // =========================================================
    // Base LogicCommand
    // =========================================================
    class LogicCommand : public Command {
    public:
        ~LogicCommand() override = default;
    };

    using LogicCommandPtr = std::unique_ptr<LogicCommand>;

    // =========================================================
    // Scene / Game State
    // =========================================================
    class SceneChangeCommand final : public LogicCommand {
    public:
        explicit SceneChangeCommand(std::string sceneName)
            : m_sceneName(std::move(sceneName)) {
        }

        const std::string &sceneName() const noexcept { return m_sceneName; }

    private:
        std::string m_sceneName;
    };

    class PauseGameCommand final : public LogicCommand {
    };

    // Resets the match to its initial state (handled only once the game is over).
    class RestartCommand final : public LogicCommand {
    };

    // Quick-save / quick-load the current match to a fixed slot (F5 / F9).
    class SaveGameCommand final : public LogicCommand {
    };
    class LoadGameCommand final : public LogicCommand {
    };

    // Replay: toggle recording (writes the log on stop), or restart + play back the
    // saved replay (F6 / F7).
    class ToggleRecordCommand final : public LogicCommand {
    };
    class PlayReplayCommand final : public LogicCommand {
    };

    class ResumeGameCommand final : public LogicCommand {
    };

    class ToggleDebugDrawCommand final : public LogicCommand {
    };

    // =========================================================
    // Selection
    // =========================================================
    class SelectCommand final : public LogicCommand {
    public:
        // additive = shift held (add/toggle into the current selection).
        // sameType = ctrl-click or double-click (select all units of that type).
        SelectCommand(const core::model::Vector2D &first, const core::model::Vector2D &second,
                      bool additive = false, bool sameType = false)
            : m_area(first, second), m_additive(additive), m_sameType(sameType) {
        }

        core::model::Rect area() const noexcept { return m_area; }
        bool additive() const noexcept { return m_additive; }
        bool sameType() const noexcept { return m_sameType; }

    private:
        core::model::Rect m_area;
        bool m_additive { false };
        bool m_sameType { false };
    };

    class SelectUnitCommand final : public LogicCommand {
    public:
        explicit SelectUnitCommand(int unitId)
            : m_unitId(unitId) {
        }

        int unitId() const noexcept { return m_unitId; }

    private:
        int m_unitId;
    };

    class ClearSelectionCommand final : public LogicCommand {
    };

    // =========================================================
    // Unit Orders
    // =========================================================
    class MoveCommand final : public LogicCommand {
    public:
        MoveCommand(core::model::Vector2D target, bool append = false)
            : m_target(target), m_append(append) {
        }

        core::model::Vector2D target() const noexcept { return m_target; }
        bool append() const noexcept { return m_append; }

    private:
        core::model::Vector2D m_target;
        bool m_append { false };
    };

    class StopCommand final : public LogicCommand {
    public:
        explicit StopCommand(int unitId)
            : m_unitId(unitId) {
        }

        int unitId() const noexcept { return m_unitId; }

    private:
        int m_unitId;
    };

    class HoldPositionCommand final : public LogicCommand {
    public:
        explicit HoldPositionCommand(int unitId)
            : m_unitId(unitId) {
        }

        int unitId() const noexcept { return m_unitId; }

    private:
        int m_unitId;
    };

    class AttackCommand final : public LogicCommand {
    public:
        // targetEntityId optionally names the exact element under the cursor; the
        // logic prefers it over positional resolution when it is a live, valid
        // target near the click (see GameLogicManager::handleAttackCommand).
        explicit AttackCommand(core::model::Vector2D target, bool append = false,
                               core::ecs::EntityId targetEntityId = core::ecs::InvalidEntityId)
            : m_target(target), m_targetEntityId(targetEntityId),
              m_hasWorldTarget(true), m_append(append) {
        }

        AttackCommand(int unitId, int targetId, bool append = false)
            : m_unitId(unitId), m_targetId(targetId), m_append(append) {
        }

        int unitId() const noexcept { return m_unitId; }
        int targetId() const noexcept { return m_targetId; }
        core::ecs::EntityId targetEntityId() const noexcept { return m_targetEntityId; }
        core::model::Vector2D target() const noexcept { return m_target; }
        bool hasWorldTarget() const noexcept { return m_hasWorldTarget; }
        bool append() const noexcept { return m_append; }

    private:
        int m_unitId { -1 };
        int m_targetId { -1 };
        core::ecs::EntityId m_targetEntityId { core::ecs::InvalidEntityId };
        core::model::Vector2D m_target {};
        bool m_hasWorldTarget { false };
        bool m_append { false };
    };

    class AttackMoveCommand final : public LogicCommand {
    public:
        AttackMoveCommand(int unitId, core::model::Vector2D target)
            : m_unitId(unitId), m_target(target) {
        }

        int unitId() const noexcept { return m_unitId; }
        core::model::Vector2D target() const noexcept { return m_target; }

    private:
        int m_unitId;
        core::model::Vector2D m_target;
    };

    class PatrolCommand final : public LogicCommand {
    public:
        PatrolCommand(int unitId, core::model::Vector2D from, core::model::Vector2D to)
            : m_unitId(unitId), m_from(from), m_to(to) {
        }

        int unitId() const noexcept { return m_unitId; }
        core::model::Vector2D from() const noexcept { return m_from; }
        core::model::Vector2D to() const noexcept { return m_to; }

    private:
        int m_unitId;
        core::model::Vector2D m_from;
        core::model::Vector2D m_to;
    };

    // =========================================================
    // Production / Build
    // =========================================================
    class TrainUnitCommand final : public LogicCommand {
    public:
        TrainUnitCommand(int buildingId, int unitTypeId)
            : m_buildingId(buildingId), m_unitTypeId(unitTypeId) {
        }

        int buildingId() const noexcept { return m_buildingId; }
        int unitTypeId() const noexcept { return m_unitTypeId; }

    private:
        int m_buildingId;
        int m_unitTypeId;
    };

    class BuildCommand final : public LogicCommand {
    public:
        BuildCommand(int buildingTypeId, core::model::Vector2D position)
            : m_buildingTypeId(buildingTypeId), m_position(position) {
        }

        int buildingTypeId() const noexcept { return m_buildingTypeId; }
        core::model::Vector2D position() const noexcept { return m_position; }

    private:
        int m_buildingTypeId;
        core::model::Vector2D m_position;
    };

    class ControlGroupSelectCommand final : public LogicCommand {
    public:
        explicit ControlGroupSelectCommand(uint16_t groupId) : m_groupId(groupId) {
        }

        uint16_t groupId() const noexcept { return m_groupId; }

    private:
        uint16_t m_groupId;
    };

    class ControlGroupAssignCommand final : public LogicCommand {
    public:
        explicit ControlGroupAssignCommand(uint16_t groupId) : m_groupId(groupId) {
        }

        uint16_t groupId() const noexcept { return m_groupId; }

    private:
        uint16_t m_groupId;
    };

    class ControlGroupAddCommand final : public LogicCommand {
    public:
        explicit ControlGroupAddCommand(uint16_t groupId) : m_groupId(groupId) {
        }

        uint16_t groupId() const noexcept { return m_groupId; }

    private:
        uint16_t m_groupId;
    };

    class CancelProductionCommand final : public LogicCommand {
    public:
        explicit CancelProductionCommand(int buildingId)
            : m_buildingId(buildingId) {
        }

        int buildingId() const noexcept { return m_buildingId; }

    private:
        int m_buildingId;
    };

    // =========================================================
    // Resource
    // =========================================================
    class GatherCommand final : public LogicCommand {
    public:
        explicit GatherCommand(core::model::Vector2D target)
            : m_target(target), m_hasWorldTarget(true) {
        }

        GatherCommand(int workerId, int resourceId)
            : m_workerId(workerId), m_resourceId(resourceId) {
        }

        int workerId() const noexcept { return m_workerId; }
        int resourceId() const noexcept { return m_resourceId; }
        core::model::Vector2D target() const noexcept { return m_target; }
        bool hasWorldTarget() const noexcept { return m_hasWorldTarget; }

    private:
        int m_workerId { -1 };
        int m_resourceId { -1 };
        core::model::Vector2D m_target {};
        bool m_hasWorldTarget { false };
    };

    class ReturnResourceCommand final : public LogicCommand {
    public:
        explicit ReturnResourceCommand(int workerId)
            : m_workerId(workerId) {
        }

        int workerId() const noexcept { return m_workerId; }

    private:
        int m_workerId;
    };

    class RepairCommand final : public LogicCommand {
    public:
        RepairCommand(int workerId, int targetId)
            : m_workerId(workerId), m_targetId(targetId) {
        }

        int workerId() const noexcept { return m_workerId; }
        int targetId() const noexcept { return m_targetId; }

    private:
        int m_workerId;
        int m_targetId;
    };

    // =========================================================
    // Ability
    // =========================================================
    class UseAbilityCommand final : public LogicCommand {
    public:
        UseAbilityCommand(const int casterId, const int abilityId, core::model::Vector2D target)
            : m_casterId(casterId),
              m_abilityId(abilityId),
              m_target(target) {
        }

        int casterId() const noexcept { return m_casterId; }
        int abilityId() const noexcept { return m_abilityId; }
        core::model::Vector2D target() const noexcept { return m_target; }

    private:
        int m_casterId;
        int m_abilityId;
        core::model::Vector2D m_target;
    };

    // =========================================================
    // Tick / Network
    // =========================================================
    class TickCommand final : public LogicCommand {
    public:
        explicit TickCommand(uint32_t tick)
            : m_tick(tick) {
        }

        uint32_t tick() const noexcept { return m_tick; }

    private:
        uint32_t m_tick;
    };

    class NetworkCommand final : public LogicCommand {
    public:
        NetworkCommand(uint32_t playerId, LogicCommandPtr payload)
            : m_playerId(playerId), m_payload(std::move(payload)) {
        }

        uint32_t playerId() const noexcept { return m_playerId; }
        const LogicCommand *payload() const noexcept { return m_payload.get(); }

    private:
        uint32_t m_playerId;
        LogicCommandPtr m_payload;
    };


    class ChangeLogicManagerCommand final : public LogicCommand {
    public:
        explicit ChangeLogicManagerCommand(
            std::shared_ptr<manager::ILogicManager> logicManager)
            : m_logicManager(std::move(logicManager)) {
        }

        const std::shared_ptr<manager::ILogicManager> &logic() const {
            return m_logicManager;
        }

    private:
        std::shared_ptr<manager::ILogicManager> m_logicManager;
    };
} // namespace rts::core::command
