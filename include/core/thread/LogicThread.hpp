//
// Created by black on 25. 12. 6..
//
// include/rts/core/LogicThread.hpp
#pragma once

#include <memory>

#include <core/command/CommandRouterBase.hpp>
#include <core/thread/ThreadBase.hpp>
#include <core/manager/ILogicManger.hpp>

namespace rts::thread {

    class LogicThread : public ThreadBase {
    public:
        explicit LogicThread(command::LogicCommandBus& bus, command::LogicCommandRouter& router)
            : m_CommandBus(bus) , m_CommandRouter(router) {}

    protected:
        void run() override;

    private:
        command::LogicCommandBus& m_CommandBus;
        command::LogicCommandRouter& m_CommandRouter;
    };

} // namespace rts::core
