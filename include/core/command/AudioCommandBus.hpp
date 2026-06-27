//
// Created by black on 26. 6. 27..
//
#pragma once
#include <memory>
#include <mutex>
#include <queue>

#include "core/command/AudioCommand.hpp"

namespace rts::core::command {
    // Thread-safe hand-off queue: gameplay/UI threads push audio commands, the
    // AudioThread drains them. Mirrors LogicCommandBus; the AudioCommandRouter
    // (in CommandRouterBase.hpp) is the separate type-dispatch half.
    class AudioCommandBus {
    public:
        void push(AudioCommandPtr cmd) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.push(std::move(cmd));
        }

        bool tryPop(AudioCommandPtr &out) {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_queue.empty())
                return false;

            out = std::move(m_queue.front());
            m_queue.pop();
            return true;
        }

        void clear() {
            std::lock_guard<std::mutex> lock(m_mutex);
            std::queue<AudioCommandPtr> empty;
            m_queue.swap(empty);
        }

    private:
        std::queue<AudioCommandPtr> m_queue;
        std::mutex m_mutex;
    };
} // namespace rts::core::command
