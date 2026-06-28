//
// Created by black on 26. 6. 28..
//
#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace rts::core::thread {

    // A minimal fixed-size task-queue thread pool. Used to resolve independent A*
    // path requests in parallel within a single logic tick: the caller submits one
    // task per request and waits on a std::latch, then applies the results in a fixed
    // order, so the simulation outcome does not depend on worker interleaving.
    //
    // A pool with zero workers is valid; callers should fall back to running tasks
    // inline (submitting to a 0-worker pool would never complete).
    class ThreadPool {
    public:
        explicit ThreadPool(std::size_t workerCount) {
            m_workers.reserve(workerCount);
            for (std::size_t i = 0; i < workerCount; ++i) {
                m_workers.emplace_back([this] { workerLoop(); });
            }
        }

        ~ThreadPool() {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_stop = true;
            }
            m_cv.notify_all();
            for (auto& worker : m_workers) {
                if (worker.joinable()) {
                    worker.join();
                }
            }
        }

        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;

        std::size_t workerCount() const noexcept { return m_workers.size(); }

        void submit(std::function<void()> task) {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_tasks.push(std::move(task));
            }
            m_cv.notify_one();
        }

    private:
        void workerLoop() {
            for (;;) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(m_mutex);
                    m_cv.wait(lock, [this] { return m_stop || !m_tasks.empty(); });
                    if (m_stop && m_tasks.empty()) {
                        return;
                    }
                    task = std::move(m_tasks.front());
                    m_tasks.pop();
                }
                task();
            }
        }

        std::vector<std::thread> m_workers;
        std::mutex m_mutex;
        std::condition_variable m_cv;
        std::queue<std::function<void()>> m_tasks;
        bool m_stop { false };
    };

} // namespace rts::core::thread
