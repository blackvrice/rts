//
// Created by black on 26. 6. 27..
//
#include <core/thread/AudioThread.hpp>
#include <core/command/CommandRouterBase.hpp>
#include <core/command/AudioCommand.hpp>
#include <core/command/AudioCommandBus.hpp>

#include <chrono>
#include <thread>

#include "core/manager/IAudioManager.hpp"

namespace rts::core::thread {
    AudioThread::AudioThread(command::AudioCommandBus& bus, command::AudioCommandRouter& router)
        : m_CommandBus(bus)
        , m_CommandRouter(router) {
        registerRouterHandlers();
    }

    void AudioThread::registerRouterHandlers() {
        m_CommandRouter.on<command::ChangeAudioManagerCommand>(
            [this](const command::ChangeAudioManagerCommand& cmd) {
                m_logic = cmd.audio();
            }
        );
    }

    void AudioThread::run() {
        using clock = std::chrono::steady_clock;

        // Audio does not need the logic thread's deterministic fixed step; it only
        // needs a steady poll to drain queued commands and let the manager service
        // playback (start/stop sounds, reclaim finished voices). ~60Hz is ample.
        constexpr auto TICK = std::chrono::milliseconds(16);
        constexpr float dt = 1.0f / 60.0f;
        auto nextTick = clock::now();

        while (isRunning()) {
            {
                // Mirrors LogicThread: the swap lock keeps command dispatch, tick, and
                // the manager's lifetime from racing a router rebuild on scene change.
                std::lock_guard<std::mutex> swapGuard(m_tickMutex);

                command::AudioCommandPtr cmd;
                while (m_CommandBus.tryPop(cmd)) {
                    m_CommandRouter.dispatch(*cmd);
                }

                if (m_logic) {
                    m_inTick.store(true, std::memory_order_release);
                    m_logic->update();
                    m_logic->tick(dt);
                    m_inTick.store(false, std::memory_order_release);
                }
            }

            nextTick += TICK;
            std::this_thread::sleep_until(nextTick);

            // If we fell badly behind (e.g. the thread was starved), resync instead of
            // bursting through a backlog of catch-up ticks.
            if (clock::now() > nextTick + TICK * 2) {
                nextTick = clock::now();
            }
        }
    }
} // namespace rts::core::thread
