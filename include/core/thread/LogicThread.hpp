//
// Created by black on 25. 12. 6..
//
// include/rts/core/LogicThread.hpp
#pragma once

#include <atomic>
#include <memory>

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

    protected:
        void run() override;

    private:
        std::atomic<bool> m_inTick{false};
        command::LogicCommandBus& m_CommandBus;
        command::LogicCommandRouter& m_CommandRouter;
        std::shared_ptr<manager::ILogicManager> m_logic = nullptr;
    };

} // namespace rts::core
