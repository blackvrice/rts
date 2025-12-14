//
// Created by black on 25. 12. 7..
//
#pragma once
#include "ecs/Registry.hpp"
#include "system/MovementSystem.hpp"
#include "system/SelectionSystem.hpp"

namespace rts::manager {

    class SystemManager {
    public:
        SystemManager() = default;

        void update(rts::ecs::Registry& reg, float dt);

        rts::system::MovementSystem movement;
        rts::system::SelectionSystem selection;
    };

} // namespace rts::manager
