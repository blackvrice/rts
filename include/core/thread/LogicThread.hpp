//
// Created by black on 25. 12. 6..
//
// include/rts/core/LogicThread.hpp
#pragma once

#include <memory>

#include <core/command/CommandRouterBase.hpp>
#include <core/thread/ThreadBase.hpp>

#include "core/command/LogicCommandBus.hpp"

namespace rts::core::manager {
    class ILogicManager;
}

namespace rts::core::thread {

    class LogicThread : public ThreadBase {
    public:
        explicit LogicThread(command::LogicCommandBus& bus, command::LogicCommandRouter& router)
            : m_CommandBus(bus) , m_CommandRouter(router) {
            router.on<command::ChangeLogicManagerCommand>(
                [this](const command::ChangeLogicManagerCommand& cmd) {
                    m_logic = cmd.logic();
                }
            );

        }

    protected:
        void run() override;

    private:
        std::atomic<bool> m_inTick{false};
        command::LogicCommandBus& m_CommandBus;
        command::LogicCommandRouter& m_CommandRouter;
        std::shared_ptr<manager::ILogicManager> m_logic = nullptr;
    };

} // namespace rts::core
