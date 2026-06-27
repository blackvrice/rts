//
// Created by black on 26. 6. 27..
//
#pragma once

#include <memory>
#include <mutex>

#include "ThreadBase.hpp"

namespace rts::core::manager {
    class IAudioManager;
}

namespace rts::core::command {
    class AudioCommandBus;
    class AudioCommandRouter;
}

namespace rts::core::thread {
    class AudioThread : public ThreadBase {
    public:
        explicit AudioThread(command::AudioCommandBus& bus, command::AudioCommandRouter& router);

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
        command::AudioCommandBus& m_CommandBus;
        command::AudioCommandRouter& m_CommandRouter;
        std::shared_ptr<manager::IAudioManager> m_logic = nullptr;
    };
}