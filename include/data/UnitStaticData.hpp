//
// Created by black on 25. 12. 23..
//

#pragma once

namespace rts::data {
    struct UnitStaticData {
        int maxHp;
        int attack;
        int defense;
        int maxMana;

        ResourceId portrait;
        ResourceId model;
    };
}