//
// Created by black on 26. 1. 4..
//

#pragma once
#include <functional>
#include <memory>
#include <string>
#include "IElement.hpp"
#include "State.hpp"
#include "Vector2D.hpp"
#include "core/data/CombatTypes.hpp"
#include "core/ecs/EntityId.hpp"

namespace rts::core::model {
    enum class ActionType {
        Idle,
        Move,
        Attack,
        Stop,
        Hold,
        Patrol,
        Gather,
        Build,
        Cast,
        Dead
    };

    namespace TeamId {
        constexpr int Neutral = 0;
        constexpr int Player  = 1;
        constexpr int Enemy   = 2;
    }

    class IGameElement : public IElement, public std::enable_shared_from_this<IGameElement>  {
    public:
        virtual ~IGameElement() = default;

        // ⏱ 로직
        virtual void tick(float dt) = 0;
        virtual const GameState& state() const = 0;

        // ===============================
        // 🎮 Action Interface
        // ===============================

        // 기본
        virtual void idle() = 0;
        virtual void stop() = 0;

        // 이동
        virtual void moveTo(const Vector2D& target) = 0;
        virtual void patrol(const Vector2D& a, const Vector2D& b) = 0;
        virtual void holdPosition() = 0;

        // 전투
        virtual void attack(IGameElement* target) = 0;
        virtual void attackMove(const Vector2D& target) = 0;
        virtual void takeDamage(float amount, IGameElement* attacker) = 0;


        // 작업
        virtual void gather(IGameElement* resource) = 0;
        virtual void build(int buildingType, const Vector2D& pos) = 0;

        // 스킬
        virtual void cast(int skillId, const Vector2D& target) = 0;

        // 상태
        virtual ActionType getAction() const = 0;
        virtual void setSelected(bool selected) = 0;

        // 이름 및 자원
        virtual std::string displayName() const { return "Element"; }
        virtual bool tryGather(int&) { return false; }
        virtual int resourceType() const { return -1; }

        // 팀
        virtual int getTeamId() const { return TeamId::Neutral; }
        virtual void setTeamId(int) {}

        // 전투 상성: 받는 쪽의 장갑 타입 (기본 Unarmored, Unit/Building이 오버라이드)
        virtual data::ArmorType armorType() const { return data::ArmorType::Unarmored; }

        // 점유 레이어 (지상/공중). 공격 측의 지상/공중 공격 가능 여부 판정에 쓰인다.
        // 기본 Ground (건물·자원 포함); 공중 유닛만 오버라이드.
        virtual data::MovementDomain movementDomain() const { return data::MovementDomain::Ground; }

        // 엔티티 핸들 (GameWorld가 addElement 시 부여)
        ecs::EntityId entityId() const noexcept { return m_entityId; }
        void setEntityId(const ecs::EntityId id) noexcept { m_entityId = id; }
        // GameWorld가 주입하는 리졸버. 타겟 EntityId를 살아있는 요소 포인터로
        // 되돌리는 데 쓰며, 타겟을 갖지 않는 요소는 기본 no-op.
        virtual void setEntityResolver(std::function<IGameElement*(ecs::EntityId)>) {}

        // GameWorld가 주입하는 원거리 발사 콜백 (origin, target, damage, weapon, team).
        // 원거리 유닛이 발사 시점에 호출; 그 외 요소는 기본 no-op.
        using ProjectileSpawner = std::function<void(
            const Vector2D&, IGameElement*, float, data::WeaponType, int, data::SplashRadii)>;
        virtual void setProjectileSpawner(ProjectileSpawner) {}

    protected:
        ecs::EntityId m_entityId { ecs::InvalidEntityId };
    };

}
