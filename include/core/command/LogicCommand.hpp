#pragma once

#include <memory>
#include <string>
#include <cstdint>



#include "Command.hpp"
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

    class ResumeGameCommand final : public LogicCommand {
    };

    class ToggleDebugDrawCommand final : public LogicCommand {
    };

    // =========================================================
    // Selection
    // =========================================================
    class SelectCommand final : public LogicCommand {
    public:
        SelectCommand(const core::model::Vector2D &first, const core::model::Vector2D &second) : m_area(first, second) {
        }


        core::model::Rect area() const noexcept { return m_area; }

    private:
        core::model::Rect m_area;
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
        MoveCommand(core::model::Vector2D target)
            : m_target(target) {
        }

        core::model::Vector2D target() const noexcept { return m_target; }

    private:
        core::model::Vector2D m_target;
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
        explicit AttackCommand(core::model::Vector2D target)
            : m_target(target), m_hasWorldTarget(true) {
        }

        AttackCommand(int unitId, int targetId)
            : m_unitId(unitId), m_targetId(targetId) {
        }

        int unitId() const noexcept { return m_unitId; }
        int targetId() const noexcept { return m_targetId; }
        core::model::Vector2D target() const noexcept { return m_target; }
        bool hasWorldTarget() const noexcept { return m_hasWorldTarget; }

    private:
        int m_unitId { -1 };
        int m_targetId { -1 };
        core::model::Vector2D m_target {};
        bool m_hasWorldTarget { false };
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
        BuildCommand(int buildingId, core::model::Vector2D position)
            : m_buildingId(buildingId), m_position(position) {
        }

        int buildingId() const noexcept { return m_buildingId; }
        core::model::Vector2D position() const noexcept { return m_position; }

    private:
        int m_buildingId;
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
        GatherCommand(int workerId, int resourceId)
            : m_workerId(workerId), m_resourceId(resourceId) {
        }

        int workerId() const noexcept { return m_workerId; }
        int resourceId() const noexcept { return m_resourceId; }

    private:
        int m_workerId;
        int m_resourceId;
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
