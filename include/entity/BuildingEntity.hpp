//
// Created by black on 25. 12. 23..
//

#pragma once
#include <data/BuildingStaticData.hpp>
#include <core/Vector2D.hpp>

namespace rts::entity {

    class BuildingEntity {
    public:
        explicit BuildingEntity(const data::BuildingStaticData& data)
            : staticData(data),
              hp(data.maxHp) {}

        // runtime only
        int hp;
        core::Vector2D position;
        bool isConstructing = false;

        const data::BuildingStaticData& staticData; // 🔥 Flyweight
    };

}