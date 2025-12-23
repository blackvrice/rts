//
// Created by black on 25. 12. 20..
//

#pragma once
#include <memory>

#include "command/CommandBus.hpp"
#include <command/CommandRouter.hpp>

namespace rts::manager {
    class IManager {
    public:
        explicit IManager(std::shared_ptr<command::CommandBus> bus, command::CommandRouter& router)
            : m_CommandBus(bus), m_CommandRouter(router) {}

        virtual ~IManager() = default;

        virtual void start() {}
        virtual void update() = 0;

    protected:
        std::shared_ptr<command::CommandBus> m_CommandBus;
        command::CommandRouter& m_CommandRouter;
    };
}
