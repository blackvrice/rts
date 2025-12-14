//
// Created by black on 25. 12. 6..
//
// src/system/MovementSystem.cpp
#include "system/MovementSystem.hpp"
#include "component/Position.hpp"
#include "component/Velocity.hpp"

namespace rts::system {

    void MovementSystem::update(ecs::Registry& reg, float dt)
    {
        auto& posStorage = reg.storage<component::Position>();
        auto& velStorage = reg.storage<component::Velocity>();

        const float stopDist = 1.0f;

        std::vector<ecs::Entity> toRemove;

        for (auto& [e, vel] : velStorage.data()) {
            if (auto* pos = posStorage.get(e)) {

                pos->value += vel.value * dt;

                sf::Vector2f diff = vel.target - pos->value;
                float len2 = diff.x * diff.x + diff.y * diff.y;

                if (len2 < stopDist * stopDist) {
                    pos->value = vel.target; // 스냅
                    toRemove.push_back(e);
                }
            }
        }

        for (auto e : toRemove)
            velStorage.remove(e);
    }


} // namespace rts::system
