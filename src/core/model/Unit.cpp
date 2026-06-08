#include <cmath>
#include <iostream>
#include <algorithm>
#include <optional>
#include <utility>

#include <core/model/Building.hpp>
#include <core/model/Unit.hpp>
#include <core/viewmodel/UnitViewModel.hpp>
#include <core/world/GridTransform.hpp>

namespace {
    constexpr float kGatherInteractRange = 72.0f;
    constexpr float kDropOffInteractRange = 82.0f;
    constexpr float kBuildInteractRange = 88.0f;

    float distanceSq(
        const rts::core::model::Vector2D& a,
        const rts::core::model::Vector2D& b) {
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        return dx * dx + dy * dy;
    }
}


namespace rts::core::model {
    Unit::Unit()
        : Unit(core::data::warriorUnitStaticData())
    {
    }

    Unit::Unit(const ::rts::UnitType unitType)
        : Unit(core::data::unitStaticDataFor(unitType))
    {
    }

    Unit::Unit(const core::data::UnitStaticData& staticData)
    {
        // ===== 기본 스탯 초기화 =====
        m_position = { 0.f, 0.f };

        applyStaticData(staticData);
        attackTimer = 0.f;

        m_action = ActionType::Idle;
        m_animationAction = ActionType::Idle;
    }

    void Unit::applyStaticData(const core::data::UnitStaticData& staticData) {
        m_unitType = staticData.unitType;
        m_displayName = staticData.displayName;
        m_maxHp = staticData.maxHp;
        m_hp = m_maxHp;
        moveSpeed = staticData.moveSpeed;
        attackRange = staticData.attackRange;
        attackDamage = staticData.attackDamage;
        attackCooldown = staticData.attackCooldown;
        m_armor = staticData.armor;
        m_sightRange = staticData.sightRange;
        m_collisionRadius = staticData.collisionRadius;
        m_buildTimeSeconds = staticData.buildTimeSeconds;
        m_weaponType = staticData.weaponType;
        m_armorType = staticData.armorType;
    }

    ActionType Unit::getAction() const {
        return m_action;
    }

    ActionType Unit::getAnimationAction() const {
        return m_animationAction;
    }

    void Unit::moveTo(const Vector2D& target) {
        if (m_action == ActionType::Dead) return;
        m_attackRetargetRequested = false;
        clearGatherState(true);
        clearAttackMoveOrder();
        clearPatrolOrder();
        m_action = ActionType::Move;
        m_animationAction = ActionType::Move;
        m_attackTargetId = ecs::InvalidEntityId;
        m_moveTarget = target;
        m_finalTargetWorld = target;
        m_gridPath.clear();
    }

    void Unit::attack(IGameElement* target) {
        beginAttack(target, false);
    }

    void Unit::setEntityResolver(std::function<IGameElement*(ecs::EntityId)> resolver) {
        m_resolveEntity = std::move(resolver);
    }

    IGameElement* Unit::attackTarget() const {
        if (!m_resolveEntity || m_attackTargetId == ecs::InvalidEntityId) {
            return nullptr;
        }
        return m_resolveEntity(m_attackTargetId);
    }

    void Unit::beginAttack(IGameElement* target, bool preserveAttackMove, bool preservePatrol) {
        if (m_action == ActionType::Dead) return;
        if (!target) return;
        if (target->getAction() == ActionType::Dead) return;
        if (target->getTeamId() == m_teamId && m_teamId != TeamId::Neutral) return;

        m_attackRetargetRequested = false;
        const bool keepAttackMove = preserveAttackMove && m_attackMoveActive;
        const bool keepPatrol = preservePatrol && m_patrolActive;
        clearGatherState(true);
        m_attackMoveActive = keepAttackMove;
        if (!keepPatrol) {
            clearPatrolOrder();
        }
        m_action = ActionType::Attack;
        m_attackTargetId = target->entityId();
        m_moveTarget = target->getPosition();
        m_finalTargetWorld = m_moveTarget;
        // The order is attack, but the sprite should run until the weapon is in range.
        m_animationAction = distanceSq(m_moveTarget, m_position) <= attackRange * attackRange
                                ? ActionType::Attack
                                : ActionType::Move;
        m_gridPath.clear();
    }

    void Unit::attackMoveEngage(IGameElement* target) {
        beginAttack(target, true);
    }

    void Unit::attackMove(const Vector2D& target) {
        if (m_action == ActionType::Dead) return;
        m_attackRetargetRequested = false;
        clearGatherState(true);
        clearPatrolOrder();
        m_attackMoveActive = true;
        m_attackMoveTarget = target;
        m_action = ActionType::Move;
        m_animationAction = ActionType::Move;
        m_attackTargetId = ecs::InvalidEntityId;
        m_moveTarget = target;
        m_finalTargetWorld = target;
        m_gridPath.clear();
    }

    void Unit::patrol(const Vector2D& a, const Vector2D& b) {
        if (m_action == ActionType::Dead) return;
        m_attackRetargetRequested = false;
        clearGatherState(true);
        clearAttackMoveOrder();
        m_patrolActive = true;
        m_patrolStart = a;
        m_patrolEnd = b;
        m_patrolDestination = b;
        m_patrolHeadingToEnd = true;
        m_action = ActionType::Patrol;
        m_animationAction = ActionType::Move;
        m_attackTargetId = ecs::InvalidEntityId;
        m_moveTarget = b;
        m_finalTargetWorld = b;
        m_gridPath.clear();
    }

    void Unit::holdEngage(IGameElement* target) {
        if (m_action == ActionType::Dead) return;
        if (!target || target->getAction() == ActionType::Dead) return;
        if (target->getTeamId() == m_teamId && m_teamId != TeamId::Neutral) return;

        const float rangeSq = attackRange * attackRange;
        if (distanceSq(target->getPosition(), m_position) > rangeSq) {
            return;
        }

        m_attackRetargetRequested = false;
        clearGatherState(true);
        clearAttackMoveOrder();
        clearPatrolOrder();
        m_action = ActionType::Hold;
        m_attackTargetId = target->entityId();
        m_animationAction = ActionType::Attack;
        m_gridPath.clear();
    }

    void Unit::patrolEngage(IGameElement* target) {
        beginAttack(target, false, true);
    }

    void Unit::tick(float dt) {
        if (m_action == ActionType::Dead)
            return;

        switch (m_action) {
        case ActionType::Move:
        case ActionType::Patrol:
            updateMove(dt);
            break;
        case ActionType::Attack:
            updateAttack(dt);
            break;
        case ActionType::Hold:
            updateHold(dt);
            break;
        case ActionType::Gather:
            updateGather(dt);
            break;
        case ActionType::Build:
            updateBuild(dt);
            break;
        default:
            break;
        }
    }

    void Unit::updateHold(float dt) {
        if (m_action != ActionType::Hold) {
            return;
        }

        IGameElement* target = attackTarget();
        if (!target || target->getAction() == ActionType::Dead) {
            m_attackTargetId = ecs::InvalidEntityId;
            m_animationAction = ActionType::Hold;
            return;
        }

        const float rangeSq = attackRange * attackRange;
        if (distanceSq(target->getPosition(), m_position) > rangeSq) {
            // Hold position never chases; targets outside weapon range are released.
            m_attackTargetId = ecs::InvalidEntityId;
            m_animationAction = ActionType::Hold;
            return;
        }

        m_animationAction = ActionType::Attack;
        attackTimer -= dt;
        if (attackTimer <= 0.f) {
            attackTimer = attackCooldown;
            target->takeDamage(attackDamage, this);
        }
    }

    void Unit::updateMove(float dt)
    {
        if (m_action != ActionType::Move && m_action != ActionType::Patrol) return;

        Vector2D delta{ m_moveTarget.x - m_position.x, m_moveTarget.y - m_position.y };
        float distSq = delta.x * delta.x + delta.y * delta.y;

        constexpr float ARRIVE_EPS = 1.0f; // 타일 중심으로 붙는 오차
        if (distSq <= ARRIVE_EPS * ARRIVE_EPS) {
            m_position = m_moveTarget;
            if (m_action == ActionType::Patrol && m_patrolActive) {
                advancePatrolDestination();
                m_action = ActionType::Idle;
                m_animationAction = ActionType::Idle;
                return;
            }
            m_action = ActionType::Idle;
            m_animationAction = ActionType::Idle;
            m_attackMoveActive = false;
            return;
        }

        float dist = std::sqrt(distSq);
        Vector2D dir{ delta.x / dist, delta.y / dist };

        float step = moveSpeed * dt;
        if (step >= dist) {
            m_position = m_moveTarget;
        } else {
            m_position.x += dir.x * step;
            m_position.y += dir.y * step;
        }
    }

    void Unit::updateMove(float dt, const world::GridTransform& tf)
    {
        if (m_action != ActionType::Move && m_action != ActionType::Patrol) return;

        Vector2D target;

        if (!m_gridPath.empty()) {
            target = tf.gridToWorldCenter(m_gridPath.front());
        } else {
            target = m_finalTargetWorld;
        }

        Vector2D delta{ target.x - m_position.x, target.y - m_position.y };
        float distSq = delta.x*delta.x + delta.y*delta.y;

        constexpr float ARRIVE_EPS = 1.0f; // 타일 중심으로 붙는 오차
        if (distSq <= ARRIVE_EPS*ARRIVE_EPS) {
            if (!m_gridPath.empty()) {
                m_gridPath.pop_front();
                return; // 다음 노드로 계속
            }
            m_position = target;
            if (m_action == ActionType::Patrol && m_patrolActive) {
                advancePatrolDestination();
                m_action = ActionType::Idle;
                m_animationAction = ActionType::Idle;
                return;
            }
            m_action = ActionType::Idle;
            m_animationAction = ActionType::Idle;
            m_attackMoveActive = false;
            return;
        }

        float dist = std::sqrt(distSq);
        Vector2D dir{ delta.x / dist, delta.y / dist };

        float step = moveSpeed * dt;
        if (step >= dist) {
            m_position = target;
        } else {
            m_position.x += dir.x * step;
            m_position.y += dir.y * step;
        }
    }



    void Unit::updateAttack(float dt) {
        if (m_action != ActionType::Attack)
            return;

        IGameElement* target = attackTarget();
        if (!target || target->getAction() == ActionType::Dead) {
            m_attackTargetId = ecs::InvalidEntityId;
            // Direct attacks ask the logic layer for a nearby replacement target;
            // attack-move and patrol keep using their existing search/resume flow.
            m_attackRetargetRequested = !m_attackMoveActive && !m_patrolActive;
            m_action = ActionType::Idle;
            m_animationAction = ActionType::Idle;
            return;
        }

        Vector2D targetPos = target->getPosition();
        m_moveTarget = targetPos;
        m_finalTargetWorld = targetPos;

        float distSq = distanceSq(targetPos, m_position);
        float rangeSq = attackRange * attackRange;

        if (distSq > rangeSq) {
            m_animationAction = ActionType::Move;
            const float dist = std::sqrt(distSq);
            if (dist <= 0.0f) {
                return;
            }

            const Vector2D dir{(targetPos.x - m_position.x) / dist, (targetPos.y - m_position.y) / dist};
            float advance = moveSpeed * dt;
            const float maxAdvance = dist - attackRange;
            if (advance > maxAdvance) {
                advance = maxAdvance;
            }

            // Attack chase stops at weapon range instead of entering the target collision radius.
            if (advance > 0.0f) {
                m_position.x += dir.x * advance;
                m_position.y += dir.y * advance;
            }
            return;
        }

        m_animationAction = ActionType::Attack;
        attackTimer -= dt;
        if (attackTimer <= 0.f) {
            attackTimer = attackCooldown;
            target->takeDamage(attackDamage, this);
        }
    }

    void Unit::takeDamage(float amount, IGameElement* attacker) {
        if (m_action == ActionType::Dead)
            return;

        // Armor is static unit data and mitigates each hit while still allowing chip damage.
        const float mitigatedAmount = std::max(1.0f, amount - m_armor);
        m_hp -= mitigatedAmount;

        if (m_hp <= 0.f) {
            m_hp = 0.f;
            m_action = ActionType::Dead;
            m_animationAction = ActionType::Dead;
            m_attackTargetId = ecs::InvalidEntityId;
            m_attackRetargetRequested = false;
            clearAttackMoveOrder();
            clearPatrolOrder();
            clearOrderQueue();
            clearGatherState(true);
            return;
        }

        if (attacker && m_action == ActionType::Hold && attacker->getTeamId() != m_teamId) {
            holdEngage(attacker);
            return;
        }

        if (attacker && m_action != ActionType::Attack && attacker->getTeamId() != m_teamId) {
            beginAttack(attacker, m_attackMoveActive, m_patrolActive);
        }
    }

    float Unit::getHp() const {
        return m_hp;
    }

    float Unit::getMaxHp() const {
        return m_maxHp;
    }

    float Unit::getAttackDamage() const {
        return attackDamage;
    }

    float Unit::getAttackRange() const {
        return attackRange;
    }

    float Unit::getAttackCooldown() const {
        return attackCooldown;
    }

    float Unit::getMoveSpeed() const {
        return moveSpeed;
    }

    float Unit::getArmor() const {
        return m_armor;
    }

    float Unit::getSightRange() const noexcept {
        return m_sightRange;
    }

    float Unit::getCollisionRadius() const noexcept {
        return m_collisionRadius;
    }

    float Unit::getBuildTimeSeconds() const noexcept {
        return m_buildTimeSeconds;
    }

    core::data::WeaponType Unit::getWeaponType() const noexcept {
        return m_weaponType;
    }

    core::data::ArmorType Unit::getArmorType() const noexcept {
        return m_armorType;
    }

    ::rts::UnitType Unit::unitType() const noexcept {
        return m_unitType;
    }

    bool Unit::isWorker() const noexcept {
        return m_unitType == ::rts::UnitType::Worker;
    }

    bool Unit::hasResourceDeliveryReady() const noexcept {
        return m_action == ActionType::Gather &&
               m_gatherState.phase == GatherPhase::DropResource &&
               m_gatherState.deliveryReady &&
               m_gatherState.carryingAmount > 0;
    }

    std::optional<Unit::ResourceDelivery> Unit::takeReadyResourceDelivery() {
        if (!hasResourceDeliveryReady()) {
            return std::nullopt;
        }

        ResourceDelivery delivery {
            m_gatherState.carryingType,
            m_gatherState.carryingAmount
        };

        m_gatherState.carryingAmount = 0;
        m_gatherState.deliveryReady = false;

        auto* resource = m_gatherState.targetResource;
        auto* dropOff = m_gatherState.targetDropOff;
        if (resource &&
            dropOff &&
            resource->getAction() != ActionType::Dead &&
            dropOff->getAction() != ActionType::Dead &&
            resource->reserveGatherSlot(*this)) {
            m_gatherState.phase = GatherPhase::MoveToResource;
            m_gatherState.gatherProgressSeconds = 0.0f;
            m_animationAction = ActionType::Move;
            m_moveTarget = resource->getPosition();
            m_finalTargetWorld = m_moveTarget;
        } else {
            const auto savedType = m_gatherState.carryingType;
            clearGatherState(false);
            m_gatherState.carryingType = savedType;
            m_gatherState.phase = GatherPhase::NeedNewResource;
            m_animationAction = ActionType::Idle;
        }

        return delivery;
    }

    std::string Unit::displayName() const {
        return m_displayName;
    }

    // ===== IElement / IGameElement 구현 =====

    void Unit::update() {
        // tick은 외부에서 호출
    }

    void Unit::idle() {
        if (m_action == ActionType::Dead) return;
        m_attackRetargetRequested = false;
        clearGatherState(true);
        clearAttackMoveOrder();
        clearPatrolOrder();
        m_action = ActionType::Idle;
        m_animationAction = ActionType::Idle;
        m_attackTargetId = ecs::InvalidEntityId;
        m_gridPath.clear();
    }

    Vector2D Unit::getPosition() const {
        return m_position;
    }

    void Unit::setPosition(const Vector2D& pos) {
        m_position = pos;
    }

    void Unit::holdPosition() {
        if (m_action == ActionType::Dead) return;
        m_attackRetargetRequested = false;
        clearGatherState(true);
        clearAttackMoveOrder();
        clearPatrolOrder();
        m_action = ActionType::Hold;
        m_animationAction = ActionType::Hold;
        m_attackTargetId = ecs::InvalidEntityId;
        m_gridPath.clear();
    }

    void Unit::setSelected(bool selected) {
        m_state.selected = selected;
    }

    int Unit::getTeamId() const {
        return m_teamId;
    }

    void Unit::setTeamId(int teamId) {
        m_teamId = teamId;
    }

    void Unit::setPath(path::Path p) {
        m_path = std::move(p);
        m_pathIndex = 0;
        // 상태 Move로 전환 등...
    }

    void Unit::setMoveTargetWithPath(const std::vector<path::GridPos>& gridPath,
                                 const Vector2D& finalWorldTarget)
    {
        if (m_action == ActionType::Dead) return;
        m_attackRetargetRequested = false;
        clearGatherState(true);
        m_gridPath.clear();
        // PathManager returns the start cell too; movement should begin at the next cell.
        for (std::size_t i = 1; i < gridPath.size(); ++i) {
            m_gridPath.push_back(gridPath[i]);
        }

        m_finalTargetWorld = finalWorldTarget;
        m_moveTarget = finalWorldTarget;
        m_action = ActionType::Move;
        m_animationAction = ActionType::Move;
        m_attackTargetId = ecs::InvalidEntityId;
    }

    void Unit::setAttackMoveTargetWithPath(const std::vector<path::GridPos>& gridPath,
                                           const Vector2D& finalWorldTarget)
    {
        if (m_action == ActionType::Dead) return;
        m_attackRetargetRequested = false;
        clearGatherState(true);
        m_gridPath.clear();
        for (std::size_t i = 1; i < gridPath.size(); ++i) {
            m_gridPath.push_back(gridPath[i]);
        }

        m_attackMoveActive = true;
        m_attackMoveTarget = finalWorldTarget;
        m_finalTargetWorld = finalWorldTarget;
        m_moveTarget = finalWorldTarget;
        m_action = ActionType::Move;
        m_animationAction = ActionType::Move;
        m_attackTargetId = ecs::InvalidEntityId;
    }

    void Unit::setPatrolRouteWithPath(const std::vector<path::GridPos>& gridPath,
                                      const Vector2D& finalWorldTarget,
                                      const Vector2D& from,
                                      const Vector2D& to)
    {
        if (m_action == ActionType::Dead) return;
        m_attackRetargetRequested = false;
        clearGatherState(true);
        clearAttackMoveOrder();
        m_gridPath.clear();
        for (std::size_t i = 1; i < gridPath.size(); ++i) {
            m_gridPath.push_back(gridPath[i]);
        }

        m_patrolActive = true;
        m_patrolStart = from;
        m_patrolEnd = to;
        m_patrolDestination = finalWorldTarget;
        m_patrolHeadingToEnd = true;
        m_finalTargetWorld = finalWorldTarget;
        m_moveTarget = finalWorldTarget;
        m_action = ActionType::Patrol;
        m_animationAction = ActionType::Move;
        m_attackTargetId = ecs::InvalidEntityId;
    }

    void Unit::setPatrolTargetWithPath(const std::vector<path::GridPos>& gridPath,
                                       const Vector2D& finalWorldTarget)
    {
        if (m_action == ActionType::Dead || !m_patrolActive) return;
        m_attackRetargetRequested = false;
        clearGatherState(true);
        m_gridPath.clear();
        for (std::size_t i = 1; i < gridPath.size(); ++i) {
            m_gridPath.push_back(gridPath[i]);
        }

        m_patrolDestination = finalWorldTarget;
        m_finalTargetWorld = finalWorldTarget;
        m_moveTarget = finalWorldTarget;
        m_action = ActionType::Patrol;
        m_animationAction = ActionType::Move;
        m_attackTargetId = ecs::InvalidEntityId;
    }

    const Vector2D& Unit::finalTargetWorld() const noexcept {
        return m_finalTargetWorld;
    }

    void Unit::enqueueOrder(const UnitOrder& order) {
        if (m_action == ActionType::Dead) return;
        m_orderQueue.push_back(order);
    }

    void Unit::clearOrderQueue() {
        m_orderQueue.clear();
    }

    bool Unit::hasQueuedOrders() const noexcept {
        return !m_orderQueue.empty();
    }

    std::optional<UnitOrder> Unit::popNextOrder() {
        if (m_orderQueue.empty()) {
            return std::nullopt;
        }

        auto order = m_orderQueue.front();
        m_orderQueue.pop_front();
        return order;
    }


    void Unit::stop() {
        if (m_action == ActionType::Dead) return;
        m_attackRetargetRequested = false;
        clearGatherState(true);
        clearAttackMoveOrder();
        clearPatrolOrder();
        m_action = ActionType::Idle;
        m_animationAction = ActionType::Idle;
        m_attackTargetId = ecs::InvalidEntityId;
        m_gridPath.clear();
    }

    const GameState& Unit::state() const {
        return m_state;
    }

    void Unit::gather(IGameElement* resource) {
        gather(dynamic_cast<ResourceNode*>(resource), nullptr);
    }

    void Unit::gather(ResourceNode* resource, Building* dropOff) {
        if (m_action == ActionType::Dead) return;

        m_attackRetargetRequested = false;
        clearGatherState(true);
        clearAttackMoveOrder();
        clearPatrolOrder();
        if (!isWorker() ||
            !resource ||
            !dropOff ||
            resource->isDepleted() ||
            dropOff->getAction() == ActionType::Dead ||
            !resource->reserveGatherSlot(*this)) {
            m_action = ActionType::Idle;
            m_animationAction = ActionType::Idle;
            return;
        }

        m_action = ActionType::Gather;
        m_animationAction = ActionType::Move;
        m_attackTargetId = ecs::InvalidEntityId;
        m_gridPath.clear();
        m_gatherState = WorkerGatherState {
            .targetResource = resource,
            .targetDropOff = dropOff,
            .carryingType = resource->type(),
            .carryingAmount = 0,
            .maxCarryAmount = resource->gatherAmountPerTrip(),
            .gatherProgressSeconds = 0.0f,
            .phase = GatherPhase::MoveToResource,
            .deliveryReady = false
        };
        m_moveTarget = resource->getPosition();
        m_finalTargetWorld = m_moveTarget;
    }

    void Unit::updateGather(float dt) {
        // Redirect phases are handled by GameLogicManager each tick
        if (m_gatherState.phase == GatherPhase::NeedNewResource ||
            m_gatherState.phase == GatherPhase::NeedNewDropOff) {
            return;
        }

        auto* resource = m_gatherState.targetResource;
        auto* dropOff = m_gatherState.targetDropOff;
        if (!isWorker() || !resource) {
            clearGatherState(true);
            m_action = ActionType::Idle;
            m_animationAction = ActionType::Idle;
            return;
        }

        if (!dropOff || dropOff->getAction() == ActionType::Dead) {
            if (m_gatherState.carryingAmount > 0) {
                m_gatherState.phase = GatherPhase::NeedNewDropOff;
                m_gatherState.targetDropOff = nullptr;
            } else {
                clearGatherState(true);
                m_action = ActionType::Idle;
                m_animationAction = ActionType::Idle;
            }
            return;
        }

        if ((m_gatherState.phase == GatherPhase::MoveToResource ||
             m_gatherState.phase == GatherPhase::Gathering) &&
            resource->isDepleted()) {
            clearGatherState(true);
            m_action = ActionType::Idle;
            m_animationAction = ActionType::Idle;
            return;
        }

        switch (m_gatherState.phase) {
            case GatherPhase::MoveToResource:
                if (!resource->hasGatherReservation(*this) &&
                    !resource->reserveGatherSlot(*this)) {
                    m_gatherState.phase = GatherPhase::NeedNewResource;
                    return;
                }

                m_animationAction = ActionType::Move;
                if (moveToward(resource->getPosition(), kGatherInteractRange, dt)) {
                    m_gatherState.phase = GatherPhase::Gathering;
                    m_gatherState.gatherProgressSeconds = 0.0f;
                    m_animationAction = ActionType::Attack;
                }
                break;

            case GatherPhase::Gathering: {
                m_animationAction = ActionType::Attack;
                m_gatherState.gatherProgressSeconds += dt;
                if (m_gatherState.gatherProgressSeconds < resource->gatherDurationSeconds()) {
                    break;
                }

                int gatheredAmount = 0;
                if (!resource->tryGather(gatheredAmount) || gatheredAmount <= 0) {
                    clearGatherState(true);
                    m_action = ActionType::Idle;
                    m_animationAction = ActionType::Idle;
                    return;
                }

                resource->releaseGatherSlot(*this);
                m_gatherState.carryingType = resource->type();
                m_gatherState.carryingAmount = std::min(
                    gatheredAmount,
                    m_gatherState.maxCarryAmount
                );
                m_gatherState.phase = GatherPhase::MoveToDropOff;
                m_gatherState.gatherProgressSeconds = 0.0f;
                m_animationAction = ActionType::Move;
                m_moveTarget = dropOff->getPosition();
                m_finalTargetWorld = m_moveTarget;
                break;
            }

            case GatherPhase::MoveToDropOff:
                m_animationAction = ActionType::Move;
                if (moveToward(dropOff->getPosition(), kDropOffInteractRange, dt)) {
                    m_gatherState.phase = GatherPhase::DropResource;
                    m_gatherState.deliveryReady = true;
                    m_animationAction = ActionType::Idle;
                }
                break;

            case GatherPhase::DropResource:
                m_animationAction = ActionType::Idle;
                break;

            case GatherPhase::None:
                clearGatherState(true);
                m_action = ActionType::Idle;
                m_animationAction = ActionType::Idle;
                break;
        }
    }

    bool Unit::moveToward(const Vector2D& target, float stopDistance, float dt) {
        Vector2D delta{ target.x - m_position.x, target.y - m_position.y };
        const float distSq = delta.x * delta.x + delta.y * delta.y;
        const float stopDistanceSq = stopDistance * stopDistance;
        if (distSq <= stopDistanceSq) {
            return true;
        }

        const float dist = std::sqrt(distSq);
        if (dist <= 0.0f) {
            return true;
        }

        const Vector2D dir{ delta.x / dist, delta.y / dist };
        const float advance = std::min(moveSpeed * dt, std::max(0.0f, dist - stopDistance));
        if (advance <= 0.0f) {
            return true;
        }

        m_position.x += dir.x * advance;
        m_position.y += dir.y * advance;
        return (dist - advance) <= stopDistance + 0.5f;
    }

    void Unit::clearGatherState(bool releaseReservation) {
        if (releaseReservation && m_gatherState.targetResource) {
            m_gatherState.targetResource->releaseGatherSlot(*this);
        }

        m_gatherState = WorkerGatherState {};
        // Every action-transition entry point clears gather state, so cancelling an
        // in-progress build order here keeps the two worker jobs mutually exclusive.
        // buildAt() reassigns m_buildTarget after its own clearGatherState() call.
        m_buildTarget = nullptr;
    }

    void Unit::clearAttackMoveOrder() {
        m_attackMoveActive = false;
        m_attackMoveTarget = {};
    }

    void Unit::clearPatrolOrder() {
        m_patrolActive = false;
        m_patrolStart = {};
        m_patrolEnd = {};
        m_patrolDestination = {};
        m_patrolHeadingToEnd = true;
    }

    void Unit::advancePatrolDestination() {
        if (!m_patrolActive) {
            return;
        }

        m_patrolHeadingToEnd = !m_patrolHeadingToEnd;
        m_patrolDestination = m_patrolHeadingToEnd ? m_patrolEnd : m_patrolStart;
    }

    void Unit::buildAt(Building* site) {
        if (m_action == ActionType::Dead) return;
        m_attackRetargetRequested = false;
        clearGatherState(true);
        clearAttackMoveOrder();
        clearPatrolOrder();
        if (!isWorker() || !site || site->isComplete() ||
            site->getAction() == ActionType::Dead) {
            m_buildTarget = nullptr;
            m_action = ActionType::Idle;
            m_animationAction = ActionType::Idle;
            return;
        }

        m_buildTarget = site;
        m_action = ActionType::Build;
        m_animationAction = ActionType::Move;
        m_attackTargetId = ecs::InvalidEntityId;
        m_gridPath.clear();
        m_moveTarget = site->getPosition();
        m_finalTargetWorld = m_moveTarget;
    }

    void Unit::updateBuild(float dt) {
        auto* site = m_buildTarget;
        if (!isWorker() || !site || site->getAction() == ActionType::Dead) {
            m_buildTarget = nullptr;
            m_action = ActionType::Idle;
            m_animationAction = ActionType::Idle;
            return;
        }

        if (site->isComplete()) {
            m_buildTarget = nullptr;
            m_action = ActionType::Idle;
            m_animationAction = ActionType::Idle;
            return;
        }

        m_animationAction = ActionType::Move;
        if (moveToward(site->getPosition(), kBuildInteractRange, dt)) {
            // In range: pour work into the site. Construction completes inside
            // advanceConstruction once accumulated progress reaches build time.
            m_animationAction = ActionType::Attack;
            if (site->advanceConstruction(dt)) {
                m_buildTarget = nullptr;
                m_action = ActionType::Idle;
                m_animationAction = ActionType::Idle;
            }
        }
    }

    bool Unit::isNeedingResourceRedirect() const noexcept {
        return m_action == ActionType::Gather &&
               m_gatherState.phase == GatherPhase::NeedNewResource;
    }

    bool Unit::isNeedingDropOffRedirect() const noexcept {
        return m_action == ActionType::Gather &&
               m_gatherState.phase == GatherPhase::NeedNewDropOff;
    }

    bool Unit::isAttackMoveActive() const noexcept {
        return m_attackMoveActive;
    }

    bool Unit::isAttackMoveSearching() const noexcept {
        return m_attackMoveActive &&
               (m_action == ActionType::Move || m_action == ActionType::Idle);
    }

    bool Unit::needsAttackMoveResume() const noexcept {
        if (!m_attackMoveActive || m_action != ActionType::Idle) {
            return false;
        }

        constexpr float kResumeEpsilonSq = 4.0f;
        return distanceSq(m_position, m_attackMoveTarget) > kResumeEpsilonSq;
    }

    bool Unit::isHoldingPosition() const noexcept {
        return m_action == ActionType::Hold;
    }

    bool Unit::isPatrolActive() const noexcept {
        return m_patrolActive;
    }

    bool Unit::isPatrolSearching() const noexcept {
        return m_patrolActive &&
               (m_action == ActionType::Patrol || m_action == ActionType::Idle);
    }

    bool Unit::needsPatrolResume() const noexcept {
        return m_patrolActive && m_action == ActionType::Idle;
    }

    bool Unit::needsAttackRetarget() const noexcept {
        return m_attackRetargetRequested &&
               m_action == ActionType::Idle &&
               !m_attackMoveActive &&
               !m_patrolActive;
    }

    void Unit::clearAttackRetarget() noexcept {
        m_attackRetargetRequested = false;
    }

    const Vector2D& Unit::attackMoveTarget() const noexcept {
        return m_attackMoveTarget;
    }

    const Vector2D& Unit::patrolDestination() const noexcept {
        return m_patrolDestination;
    }

    ResourceNode::ResourceType Unit::targetGatherType() const noexcept {
        return m_gatherState.carryingType;
    }

    void Unit::redirectToDropOff(Building* newDropOff) {
        if (!newDropOff || m_gatherState.phase != GatherPhase::NeedNewDropOff) return;
        m_gatherState.targetDropOff = newDropOff;
        m_gatherState.phase = GatherPhase::MoveToDropOff;
        m_moveTarget = newDropOff->getPosition();
        m_finalTargetWorld = m_moveTarget;
        m_animationAction = ActionType::Move;
    }

}
