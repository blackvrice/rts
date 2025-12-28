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
        explicit LogicThread(std::shared_ptr<command::LogicCommandBus> bus, std::shared_ptr<command::LogicCommandRouter> router)
            : m_CommandBus(std::move(bus)) , m_CommandRouter(std::move(router)) {}

    protected:
        void run() override;

    private:
        std::shared_ptr<command::LogicCommandBus> m_CommandBus;
        std::shared_ptr<command::LogicCommandRouter> m_CommandRouter;
    };

} // namespace rts::core
