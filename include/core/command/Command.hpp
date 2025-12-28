//
// Created by black on 25. 12. 6..
//
// include/rts/core/Command.hpp
#pragma once

namespace rts::command {

    class Command {
    public:
        virtual ~Command() = default;
    };

    using CommandPtr = std::unique_ptr<Command>;

}