//
// Created by black on 25. 12. 6..
//

// include/rts/system/MovementSystem.hpp
#pragma once

#include "ecs/Registry.hpp"

namespace rts::system {

    class MovementSystem {
    public:
        void update(rts::ecs::Registry& reg, float dt);
    };

} // namespace rts::system
