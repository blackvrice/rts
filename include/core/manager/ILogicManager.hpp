//
// Created by black on 25. 12. 20..
//

#pragma once
#include <memory>

#include <core/command/LogicCommandBus.hpp>

#include "core/command/CommandRouterBase.hpp"

namespace rts::core::manager {
    class ILogicManager {
    public:
        explicit ILogicManager(command::LogicCommandBus &bus, command::LogicCommandRouter &router)
            : m_bus(bus), m_router(router) {
        }

        virtual ~ILogicManager() = default;

        virtual void update() = 0;

        virtual void tick(float dt) = 0;

    protected:
        command::LogicCommandBus &m_bus;
        command::LogicCommandRouter &m_router;
    };
}
