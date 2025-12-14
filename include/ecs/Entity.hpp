//
// Created by black on 25. 12. 6..
//

// include/rts/ecs/Entity.hpp
#pragma once
#include <cstdint>

namespace rts::ecs {

    using Entity = std::uint32_t;
    inline constexpr Entity InvalidEntity = 0;

} // namespace rts::ecs
