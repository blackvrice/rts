//
// Created by black on 25. 12. 6..
//
// include/rts/core/LogicThread.hpp
#pragma once

#include <atomic>
#include <memory>
#include <mutex>

#include <core/thread/ThreadBase.hpp>

namespace rts::core::manager {
    class ILogicManager;
}

namespace rts::core::command {
    class LogicCommandBus;
    class LogicCommandRouter;
}

namespace rts::core::thread {

    class LogicThread : public ThreadBase {
    public:
        explicit LogicThread(command::LogicCommandBus& bus, command::LogicCommandRouter& router);

        // Held by the loop around each command-drain + tick. The SceneManager takes
        // it for the whole scene swap so the router can be cleared/rebuilt and the
        // outgoing logic manager destroyed without racing a dispatch or tick.
        std::unique_lock<std::mutex> acquireSwapLock() {
            return std::unique_lock<std::mutex>(m_tickMutex);
        }
        // (Re)registers this thread's persistent router handler. Called again after
        // the SceneManager clears the shared logic router on a scene change.
        void registerRouterHandlers();

    protected:
        void run() override;

    private:
        std::atomic<bool> m_inTick{false};
        std::mutex m_tickMutex;
        command::LogicCommandBus& m_CommandBus;
        command::LogicCommandRouter& m_CommandRouter;
        std::shared_ptr<manager::ILogicManager> m_logic = nullptr;
    };

} // namespace rts::core
