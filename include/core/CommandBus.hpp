//
// Created by black on 25. 12. 6..
//
// include/rts/core/CommandBus.hpp
#pragma once
#include <queue>
#include <memory>
#include "core/Command.hpp"

namespace rts::manager { class GameManager; }

namespace rts::core {

    class CommandBus {
    public:
        void push(std::shared_ptr<Command> cmd) {
            m_queue.push(std::move(cmd));
        }

        bool tryPop(std::shared_ptr<Command>& out) {
            if (m_queue.empty())
                return false;
            out = m_queue.front();
            m_queue.pop();
            return true;
        }

        void process(rts::manager::GameManager& game) {
            std::shared_ptr<Command> cmd;
            while (tryPop(cmd)) {
                cmd->execute(game);
            }
        }

    private:
        std::queue<std::shared_ptr<Command>> m_queue;
    };

} // namespace rts::core
