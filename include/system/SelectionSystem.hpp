//
// Created by black on 25. 12. 7..
//
#pragma once
#include "ecs/Registry.hpp"
#include "component/Selection.hpp"
#include "component/Position.hpp"

namespace rts::system {

    class SelectionSystem {
    public:
        // dt 는 선택 로직에는 필요 없지만 일관성을 위해 포함
        void update(rts::ecs::Registry& reg, float dt);
    };

}
