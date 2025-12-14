//
// Created by black on 25. 12. 7..
//

#pragma once
#include <SFML/System.hpp>

namespace rts::component {
    struct TransformComponent {
        sf::Vector2f position;
        float rotation = 0.f;
        sf::Vector2f scale = {1.f, 1.f};
    };
}