//
// Created by black on 26. 1. 4..
//

#pragma once
#include <string>
#include <vector>

#include "Vector2D.hpp"

namespace rts::core::model {

    struct EntityData {
        std::string type;     // "marine", "barracks"
        std::string name;     // Display name

        int maxHp { 0 };
        int shield { 0 };

        float buildTime { 0.f };

        // 리소스 키 (핸들 ❌)
        std::string spriteKey;
        std::string iconKey;
    };

    struct UnitData : public EntityData {
        float speed { 0.f };
        int attack { 0 };
        float attackRange { 0.f };
    };


    struct BuildData : public EntityData {
        int supply { 0 };
        Vector2D rallyPointOffset { 0.f, 0.f };
    };



}
