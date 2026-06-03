//
// Created by black on 25. 12. 6..
//


#pragma once
#include <memory>
#include <mutex>
#include <queue>

#include "core/command/LogicCommand.hpp"

namespace rts::core::command {
    class LogicCommandBus {
    public:

        void push(LogicCommandPtr cmd) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.push(std::move(cmd));
        }

        bool tryPop(LogicCommandPtr &out) {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_queue.empty())
                return false;

            out = std::move(m_queue.front());
            m_queue.pop();
            return true;
        }

        void clear() {
            std::lock_guard<std::mutex> lock(m_mutex);
            std::queue<LogicCommandPtr> empty;
            m_queue.swap(empty);
        }

    private:
        std::queue<LogicCommandPtr> m_queue;
        std::mutex m_mutex;
    };
} // namespace rts::core
