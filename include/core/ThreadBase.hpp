//
// Created by black on 25. 12. 6..
//

#ifndef RTS_THREADBASE_HPP
#define RTS_THREADBASE_HPP

// include/rts/core/ThreadBase.hpp
#pragma once
#include <thread>
#include <atomic>

namespace rts::core {

    class ThreadBase {
    public:
        virtual ~ThreadBase() { stop(); }

        void start() {
            if (m_running) return;
            m_running = true;
            m_thread = std::thread([this] { run(); });
        }

        void stop() {
            if (!m_running) return;
            m_running = false;
            onStopRequested();
            if (m_thread.joinable())
                m_thread.join();
        }

        bool isRunning() const { return m_running; }

    protected:
        std::atomic<bool> m_running{false};
        std::thread m_thread;

        virtual void run() = 0;
        virtual void onStopRequested() {}
    };

} // namespace rts::core


#endif //RTS_THREADBASE_HPP