//
// Created by black on 25. 12. 7..
//
#pragma once
#include <SFML/Graphics.hpp>

namespace rts::component {
    struct AnimationComponent {
        int frame = 0;
        int frameCount = 1;
        float frameTime = 0.1f;
        float timer = 0.f;
        int frameWidth;
        int frameHeight;
    };
}