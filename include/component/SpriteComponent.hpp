//
// Created by black on 25. 12. 7..
//

#pragma once
#include <SFML/Graphics.hpp>
namespace rts::component {
    struct SpriteComponent {
        sf::Sprite sprite;
        int layer = 1; // 0=tiles,1=units,2=shadow...
    };
}