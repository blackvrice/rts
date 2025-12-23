//
// Created by black on 25. 12. 6..
//
// src/core/LogicThread.cpp
// src/core/LogicThread.cpp
#include "../../include/thread/LogicThread.hpp"
#include <SFML/System/Clock.hpp>
#include <thread>
#include <chrono>
#include <SFML/System/Time.hpp>

#include "manager/IManager.hpp"

namespace rts::thread {

    void LogicThread::run()
    {
        using clock = std::chrono::steady_clock;
        using namespace std::chrono_literals;

        constexpr auto TICK = 16ms; // 60Hz (RTS면 20~33ms도 흔함)

        while (isRunning()) {
            auto tickStart = clock::now();

            // 1️⃣ 모든 Command 처리
            command::CommandBus::CommandPtr cmd;
            while (m_CommandBus->tryPop(cmd)) {
                m_CommandRouter->dispatch(*cmd);
            }

            // 2️⃣ 모든 Manager update
            for (auto* mgr : m_managers) {
                mgr->update();
            }

            // 3️⃣ 틱 고정
            auto elapsed = clock::now() - tickStart;
            if (elapsed < TICK) {
                std::this_thread::sleep_for(TICK - elapsed);
            }
        }
    }


} // namespace rts::core
