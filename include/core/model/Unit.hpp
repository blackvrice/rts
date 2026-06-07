//
// Created by black on 26. 1. 4..
//

#pragma once
#include <cstddef>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "IGameElement.hpp"
#include "ResourceNode.hpp"
#include "core/data/UnitStaticData.hpp"
#include "core/path/GridTypes.hpp"

namespace rts::core::world {
    struct GridTransform;
}

namespace rts::core::model {
    class Building;
    class Unit : public IGameElement{
    public:
        explicit Unit();
        explicit Unit(::rts::UnitType unitType);
        explicit Unit(const core::data::UnitStaticData& staticData);

        struct ResourceDelivery {
            ResourceNode::ResourceType type;
            int amount { 0 };
        };

        ActionType getAction() const override;
        ActionType getAnimationAction() const;

        void moveTo(const Vector2D &target) override;

        void attack(IGameElement *target) override;

        void tick(float dt) override;

        void updateMove(float dt);

        void updateMove(float dt, const world::GridTransform& tf);

        void updateAttack(float dt);

        void takeDamage(float amount, IGameElement *attacker) override;

        float getHp() const;

        float getMaxHp() const;
        float getAttackDamage() const;
        float getAttackRange() const;
        float getAttackCooldown() const;
        float getMoveSpeed() const;
        float getArmor() const;
        ::rts::UnitType unitType() const noexcept;
        bool isWorker() const noexcept;
        bool hasResourceDeliveryReady() const noexcept;
        bool isNeedingResourceRedirect() const noexcept;
        bool isNeedingDropOffRedirect() const noexcept;
        bool isAttackMoveActive() const noexcept;
        bool isAttackMoveSearching() const noexcept;
        bool needsAttackMoveResume() const noexcept;
        const Vector2D& attackMoveTarget() const noexcept;
        ResourceNode::ResourceType targetGatherType() const noexcept;
        void redirectToDropOff(Building* newDropOff);
        void attackMoveEngage(IGameElement* target);
        std::optional<ResourceDelivery> takeReadyResourceDelivery();
        std::string displayName() const override;

        void update() override;
        const GameState& state() const override;
        Vector2D getPosition() const override;
        void setPosition(const Vector2D&) override;
        void idle() override;
        void stop() override;
        void holdPosition() override;
        void patrol(const Vector2D&, const Vector2D&) override {}
        void attackMove(const Vector2D& target) override;
        void gather(IGameElement*) override;
        void gather(ResourceNode* resource, Building* dropOff);
        void build(int, const Vector2D&) override {}
        // Sends this worker to construct an existing (under-construction) building site.
        void buildAt(Building* site);
        void cast(int, const Vector2D&) override {}
        void setSelected(bool selected) override;

        int getTeamId() const override;
        void setTeamId(int teamId) override;

        void setPath(path::Path p);
        void setMoveTargetWithPath(const std::vector<path::GridPos>& gridPath,
                                   const Vector2D& finalWorldTarget);
        void setAttackMoveTargetWithPath(const std::vector<path::GridPos>& gridPath,
                                         const Vector2D& finalWorldTarget);
        const Vector2D& finalTargetWorld() const noexcept;

    private:
        enum class GatherPhase {
            None,
            MoveToResource,
            Gathering,
            MoveToDropOff,
            DropResource,
            NeedNewResource,
            NeedNewDropOff
        };

        struct WorkerGatherState {
            ResourceNode* targetResource { nullptr };
            Building* targetDropOff { nullptr };
            ResourceNode::ResourceType carryingType {};
            int carryingAmount { 0 };
            int maxCarryAmount { 10 };
            float gatherProgressSeconds { 0.0f };
            GatherPhase phase { GatherPhase::None };
            bool deliveryReady { false };
        };

        void applyStaticData(const core::data::UnitStaticData& staticData);
        void updateGather(float dt);
        void updateBuild(float dt);
        bool moveToward(const Vector2D& target, float stopDistance, float dt);
        void clearGatherState(bool releaseReservation);
        void beginAttack(IGameElement* target, bool preserveAttackMove);
        void clearAttackMoveOrder();

        std::deque<path::GridPos> m_gridPath; // 다음 노드부터 pop_front
        Vector2D m_finalTargetWorld{};
        path::Path m_path;
        size_t m_pathIndex{0};

        ActionType m_action = ActionType::Idle;
        ActionType m_animationAction = ActionType::Idle;

        Vector2D m_position{};
        Vector2D m_moveTarget{};
        Vector2D m_attackMoveTarget{};

        IGameElement* m_attackTarget = nullptr;
        bool m_attackMoveActive { false };

        ::rts::UnitType m_unitType { ::rts::UnitType::Warrior };
        WorkerGatherState m_gatherState {};
        Building* m_buildTarget { nullptr };
        std::string m_displayName { "Unit" };
        float moveSpeed = 120.f;

        float attackRange = 64.f;
        float attackCooldown = 0.8f;
        float attackTimer = 0.f;
        float attackDamage = 10.f;
        float m_armor = 0.f;

        float m_hp = 100.f;
        float m_maxHp = 100.f;

        int m_teamId{TeamId::Neutral};

        GameState m_state{};
    };
}
