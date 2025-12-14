//
// Created by black on 25. 12. 6..
//
// src/core/LogicThread.cpp
// src/core/LogicThread.cpp
#include "core/LogicThread.hpp"
#include <SFML/System/Clock.hpp>
#include <thread>
#include <chrono>
#include <SFML/System/Time.hpp>

namespace rts::core {

    void LogicThread::run()
    {
        using namespace std::chrono_literals;

        sf::Clock clock;

        while (isRunning()) {
            float dt = clock.restart().asSeconds();

            if (auto bus = m_game->commands()) {
                bus->process(*m_game);
            }

            m_game->update(dt);

            std::this_thread::sleep_for(1ms);
        }
    }

} // namespace rts::core
