//
// Created by black on 25. 12. 7..
//

// include/rts/presenter/MoveCommand.hpp
#pragma once

#include "core/Command.hpp"
#include "component/Selection.hpp"
#include "component/Position.hpp"
#include "component/Velocity.hpp"
#include "manager/GameManager.hpp"
#include <SFML/System/Vector2.hpp>
#include <cmath>

namespace rts::presenter {

    class MoveCommand final : public rts::core::Command {
    public:
        explicit MoveCommand(sf::Vector2f target)
            : m_target(target) {}

        void execute(rts::manager::GameManager& game) override {
            auto& reg = game.registry();
            auto& selStorage = reg.storage<rts::component::Selection>();
            auto& posStorage = reg.storage<rts::component::Position>();
            auto& velStorage = reg.storage<rts::component::Velocity>();

            for (auto& [e, sel] : selStorage.data()) {
                if (!sel.selected) continue;

                if (auto* pos = posStorage.get(e)) {
                    sf::Vector2f diff = m_target - pos->value;
                    float len2 = diff.x * diff.x + diff.y * diff.y;
                    if (len2 > 0.0001f) {
                        float len = std::sqrt(len2);
                        rts::component::Velocity v;
                        v.value = diff / len * 100.f; // speed 100
                        velStorage.add(e, v);
                    }
                }
            }
        }

    private:
        sf::Vector2f m_target;
    };

} // namespace rts::presenter
