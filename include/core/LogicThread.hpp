//
// Created by black on 25. 12. 6..
//
// include/rts/core/LogicThread.hpp
#pragma once

#include <memory>
#include "core/ThreadBase.hpp"
#include "manager/GameManager.hpp"

namespace rts::core {

    class LogicThread : public ThreadBase {
    public:
        explicit LogicThread(std::shared_ptr<rts::manager::GameManager> game)
            : m_game(std::move(game)) {}

    protected:
        void run() override;

    private:
        std::shared_ptr<rts::manager::GameManager> m_game;
    };

} // namespace rts::core
