//
// Created by black on 25. 12. 7..
//
#pragma once
#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Vector2.hpp>

namespace rts::model {

    struct UnitSnapshot {
        sf::Vector2f position{};
        float radius = 0.f;
        sf::Color color = sf::Color::White;
        bool selected = false;

        // 필요하다면 이후 확장 가능
        // float health = 100.f;
        // int spriteId = 0;
    };

}
