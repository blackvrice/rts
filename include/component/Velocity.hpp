//
// Created by black on 25. 12. 6..
//

#ifndef RTS_VELOCITY_HPP
#define RTS_VELOCITY_HPP
// include/rts/component/Velocity.hpp
#pragma once
#include <SFML/System/Vector2.hpp>

namespace rts::component {

    struct Velocity {
        sf::Vector2f value;   // 프레임 이동 벡터
        sf::Vector2f target;  // 도착해야 하는 목표 좌표
    };

} // namespace rts::component

#endif //RTS_VELOCITY_HPP