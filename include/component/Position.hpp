//
// Created by black on 25. 12. 6..
//

#ifndef RTS_POSITION_HPP
#define RTS_POSITION_HPP
// include/rts/component/Position.hpp
#pragma once
#include <SFML/System/Vector2.hpp>

namespace rts::component {

    struct Position {
        sf::Vector2f value{};
    };

} // namespace rts::component

#endif //RTS_POSITION_HPP