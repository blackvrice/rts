//
// Created by black on 25. 12. 6..
//
// include/rts/core/Command.hpp
#pragma once

#include <memory>

namespace rts::manager {
    class GameManager; // 전방 선언
}

namespace rts::core {

    class Command {
    public:
        virtual ~Command() = default;
        virtual void execute(rts::manager::GameManager& game) = 0;
    };

} // namespace rts::core