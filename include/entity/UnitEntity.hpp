//
// Created by black on 25. 12. 23..
//

#pragma once

namespace rts::entity {
    class UnitEntity {
        public:
        int hp;
        int attack;
        int defense;
        int mana;

        core::Vector2D position;
        bool isSelected = false;



        const unit::UnitStaticData& staticData; // 🔥 Flyweight
    };
}