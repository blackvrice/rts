//
// Created by black on 25. 12. 7..
//
#include "manager/SystemManager.hpp"

namespace rts::manager {

    void SystemManager::update(rts::ecs::Registry& reg, float dt)
    {
        movement.update(reg, dt);
        selection.update(reg, dt);
    }

} // namespace rts::manager
