//
// Created by black on 26. 1. 4..
//

#pragma once
#include <memory>
#include <string>
#include "IElement.hpp"
#include "State.hpp"
#include "Vector2D.hpp"

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
    };

}
