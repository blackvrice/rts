//
// Created by black on 25. 12. 6..
//

#ifndef RTS_RENDERINFO_HPP
#define RTS_RENDERINFO_HPP
// include/rts/component/RenderInfo.hpp
#pragma once
#include <SFML/Graphics/Color.hpp>

namespace rts::component {

    struct RenderInfo {
        sf::Color color{sf::Color::White};
        float radius{8.f};
    };

} // namespace rts::component

#endif //RTS_RENDERINFO_HPP