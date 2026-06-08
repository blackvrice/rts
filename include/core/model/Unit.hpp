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

    enum class OrderType {
        Move,
        Attack,
        AttackMove,
        Patrol,
        Gather,
        Build,
        Ability
    };

    struct UnitOrder {
        OrderType type { OrderType::Move };
        int targetEntityId { -1 };
        Vector2D targetPosition {};
        int abilityId { -1 };
        int buildingTypeId { -1 };
    };

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
        float getSightRange() const noexcept;
        float getCollisionRadius() const noexcept;
        float getBuildTimeSeconds() const noexcept;
        core::data::WeaponType getWeaponType() const noexcept;
        core::data::ArmorType getArmorType() const noexcept;
        ::rts::UnitType unitType() const noexcept;
        bool isWorker() const noexcept;
        bool hasResourceDeliveryReady() const noexcept;
        bool isNeedingResourceRedirect() const noexcept;
        bool isNeedingDropOffRedirect() const noexcept;
        bool isAttackMoveActive() const noexcept;
        bool isAttackMoveSearching() const noexcept;
        bool needsAttackMoveResume() const noexcept;
        bool isHoldingPosition() const noexcept;
        bool isPatrolActive() const noexcept;
        bool isPatrolSearching() const noexcept;
        bool needsPatrolResume() const noexcept;
        bool needsAttackRetarget() const noexcept;
        void clearAttackRetarget() noexcept;
        const Vector2D& attackMoveTarget() const noexcept;
        const Vector2D& patrolDestination() const noexcept;
        ResourceNode::ResourceType targetGatherType() const noexcept;
        void redirectToDropOff(Building* newDropOff);
        void attackMoveEngage(IGameElement* target);
        void holdEngage(IGameElement* target);
        void patrolEngage(IGameElement* target);
        std::optional<ResourceDelivery> takeReadyResourceDelivery();
        std::string displayName() const override;

        void update() override;
        const GameState& state() const override;
        Vector2D getPosition() const override;
        void setPosition(const Vector2D&) override;
        void idle() override;
        void stop() override;
        void holdPosition() override;
        void patrol(const Vector2D& a, const Vector2D& b) override;
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
        void setEntityResolver(std::function<IGameElement*(ecs::EntityId)> resolver) override;

        void setPath(path::Path p);
        void setMoveTargetWithPath(const std::vector<path::GridPos>& gridPath,
                                   const Vector2D& finalWorldTarget);
        void setAttackMoveTargetWithPath(const std::vector<path::GridPos>& gridPath,
                                         const Vector2D& finalWorldTarget);
        void setPatrolRouteWithPath(const std::vector<path::GridPos>& gridPath,
                                    const Vector2D& finalWorldTarget,
                                    const Vector2D& from,
                                    const Vector2D& to);
        void setPatrolTargetWithPath(const std::vector<path::GridPos>& gridPath,
                                     const Vector2D& finalWorldTarget);
        const Vector2D& finalTargetWorld() const noexcept;
        void enqueueOrder(const UnitOrder& order);
        void clearOrderQueue();
        bool hasQueuedOrders() const noexcept;
        std::optional<UnitOrder> popNextOrder();

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
            ecs::EntityId targetResourceId { ecs::InvalidEntityId };
            ecs::EntityId targetDropOffId { ecs::InvalidEntityId };
            ResourceNode::ResourceType carryingType {};
            int carryingAmount { 0 };
            int maxCarryAmount { 10 };
            float gatherProgressSeconds { 0.0f };
            GatherPhase phase { GatherPhase::None };
            bool deliveryReady { false };
        };

        void applyStaticData(const core::data::UnitStaticData& staticData);
        void updateHold(float dt);
        void updateGather(float dt);
        void updateBuild(float dt);
        bool moveToward(const Vector2D& target, float stopDistance, float dt);
        void clearGatherState(bool releaseReservation);
        void beginAttack(IGameElement* target, bool preserveAttackMove, bool preservePatrol = false);
        void clearAttackMoveOrder();
        void clearPatrolOrder();
        void advancePatrolDestination();
        // Resolves an EntityId to a live element via the injected resolver
        // (nullptr if dead/recycled or no resolver). Typed helpers downcast.
        IGameElement* resolveEntity(ecs::EntityId id) const;
        ResourceNode* resolveResource(ecs::EntityId id) const;
        Building* resolveBuilding(ecs::EntityId id) const;
        IGameElement* attackTarget() const;

        std::deque<path::GridPos> m_gridPath; // 다음 노드부터 pop_front
        std::deque<UnitOrder> m_orderQueue;
        Vector2D m_finalTargetWorld{};
        path::Path m_path;
        size_t m_pathIndex{0};

        ActionType m_action = ActionType::Idle;
        ActionType m_animationAction = ActionType::Idle;

        Vector2D m_position{};
        Vector2D m_moveTarget{};
        Vector2D m_attackMoveTarget{};
        Vector2D m_patrolStart{};
        Vector2D m_patrolEnd{};
        Vector2D m_patrolDestination{};

        // Attack target referenced by handle, resolved through m_resolveEntity so
        // a dead/recycled target stops resolving instead of dangling.
        ecs::EntityId m_attackTargetId { ecs::InvalidEntityId };
        std::function<IGameElement*(ecs::EntityId)> m_resolveEntity;
        bool m_attackMoveActive { false };
        bool m_patrolActive { false };
        bool m_patrolHeadingToEnd { true };
        bool m_attackRetargetRequested { false };

        ::rts::UnitType m_unitType { ::rts::UnitType::Warrior };
        WorkerGatherState m_gatherState {};
        ecs::EntityId m_buildTargetId { ecs::InvalidEntityId };
        std::string m_displayName { "Unit" };
        float moveSpeed = 120.f;

        float attackRange = 64.f;
        float attackCooldown = 0.8f;
        float attackTimer = 0.f;
        float attackDamage = 10.f;
        float m_armor = 0.f;
        float m_sightRange = 256.f;
        float m_collisionRadius = 28.f;
        float m_buildTimeSeconds = 8.f;
        core::data::WeaponType m_weaponType = core::data::WeaponType::Normal;
        core::data::ArmorType m_armorType = core::data::ArmorType::Light;

        float m_hp = 100.f;
        float m_maxHp = 100.f;

        int m_teamId{TeamId::Neutral};

        GameState m_state{};
    };
}
