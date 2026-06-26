//
// Created by black on 25. 12. 6..
//
// src/core/LogicThread.cpp
// src/core/LogicThread.cpp
#include <core/thread/LogicThread.hpp>
#include <core/command/CommandRouterBase.hpp>
#include <core/command/LogicCommand.hpp>
#include <core/command/LogicCommandBus.hpp>
#include <SFML/System/Clock.hpp>
#include <thread>
#include <chrono>
#include <SFML/System/Time.hpp>

#include "core/manager/ILogicManager.hpp"
#include "core/sim/SimClock.hpp"

namespace rts::core::thread {
    LogicThread::LogicThread(command::LogicCommandBus& bus, command::LogicCommandRouter& router)
        : m_CommandBus(bus)
        , m_CommandRouter(router) {
        registerRouterHandlers();
    }

    void LogicThread::registerRouterHandlers() {
        m_CommandRouter.on<command::ChangeLogicManagerCommand>(
            [this](const command::ChangeLogicManagerCommand& cmd) {
                m_logic = cmd.logic();
            }
        );
    }

    void LogicThread::run()
    {
        using clock = std::chrono::steady_clock;

        // Fixed 30Hz simulation step (see core/sim/SimClock.hpp). dt is constant
        // by design so the simulation is reproducible from the same tick sequence.
        constexpr auto TICK = sim::kLogicTickInterval;
        constexpr float dt = sim::kFixedDeltaSeconds;
        auto nextTick = clock::now();

        while (isRunning()) {
            {
                // The SceneManager holds m_tickMutex for the duration of a scene swap;
                // blocking here keeps command dispatch, tick, and the manager's
                // lifetime from racing the router rebuild / scope teardown.
                std::lock_guard<std::mutex> swapGuard(m_tickMutex);

                // 1️⃣ Command 처리 (이벤트)
                command::LogicCommandPtr cmd;
                while (m_CommandBus.tryPop(cmd)) {
                    m_CommandRouter.dispatch(*cmd);
                }

                if (m_logic) {
                    m_inTick.store(true, std::memory_order_release);
                    m_logic->tick(dt);
                    m_inTick.store(false, std::memory_order_release);
                }
            }
            // 3️⃣ 다음 tick 시점 계산
            nextTick += TICK;

            // 4️⃣ 정확한 대기 (드리프트 방지)
            std::this_thread::sleep_until(nextTick);

            // 5️⃣ 밀린 경우 보정 (폭주 방지)
            if (clock::now() > nextTick + TICK * 2) {
                nextTick = clock::now();
            }
        }
    }

} // namespace rts::core
