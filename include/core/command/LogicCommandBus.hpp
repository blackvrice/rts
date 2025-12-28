//
// Created by black on 25. 12. 6..
//
// include/rts/core/CommandBus.hpp
#pragma once
#include <memory>
#include <queue>
#include <core/command/LogicCommandBus.hpp>
#include <core/command/LogicCommand.hpp>

namespace rts::command {

    class LogicCommandBus {
    public:
        using CommandPtr = LogicCommandPtr;

        void push(CommandPtr cmd) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.push(std::move(cmd));
        }

        bool tryPop(CommandPtr& out) {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_queue.empty())
                return false;

            out = std::move(m_queue.front());
            m_queue.pop();
            return true;
        }

        void clear() {
            std::lock_guard<std::mutex> lock(m_mutex);
            std::queue<CommandPtr> empty;
            m_queue.swap(empty);
        }

        int push(int _cpp_par_);

    private:
        std::queue<CommandPtr> m_queue;
        std::mutex m_mutex;
    };

} // namespace rts::core
