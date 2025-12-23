//
// Created by black on 25. 12. 6..
//
// include/rts/core/LogicThread.hpp
#pragma once

#include <memory>

#include <command/CommandRouter.hpp>
#include "thread/ThreadBase.hpp"
#include "manager/IManager.hpp"

namespace rts::thread {

    class LogicThread : public ThreadBase {
    public:
        explicit LogicThread(std::shared_ptr<command::CommandBus> bus, std::shared_ptr<command::CommandRouter> router)
            : m_CommandBus(std::move(bus)) , m_CommandRouter(std::move(router)) {}

        void addManager(manager::IManager* mgr) {
            m_managers.push_back(mgr);
        }

    protected:
        void run() override;

    private:
        std::shared_ptr<command::CommandBus> m_CommandBus;
        std::shared_ptr<command::CommandRouter> m_CommandRouter;
        std::vector<manager::IManager*> m_managers;
    };

} // namespace rts::core
