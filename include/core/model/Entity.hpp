//
// Created by black on 26. 1. 4..
//


#pragma once
#include "Data.hpp"
#include "State.hpp"

namespace rts::core::model {
    class UnitEntity {
    public:
        UnitEntity(UnitId id, const UnitData& data, Vector2D pos)
            : m_data(data)
        {
            m_state.id = id;
            m_state.pos = pos;
            m_state.hp = data.maxHp;
        }

        void tick(float dt);

        UnitState& state()  { return m_state; }
        UnitData& data()  { return m_data; }

    private:
        UnitData m_data; // 🔥 공유
        UnitState m_state;
    };
}
