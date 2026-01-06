//
// Created by black on 25. 12. 20..
//

#pragma once
#include <memory>

#include <core/command/LogicCommandBus.hpp>
#include <core/command/CommandRouterBase.hpp>

#include "core/model/IViewModel.hpp"

namespace rts::manager {
    class ILogicManager {
    public:
        explicit ILogicManager(command::LogicCommandBus& bus, command::LogicCommandRouter& router)
            : m_bus(bus), m_router(router) {


        }

        virtual ~ILogicManager() = default;

        virtual void start() {}
        virtual void update() = 0;

    protected:
        command::LogicCommandBus& m_bus;
        command::LogicCommandRouter& m_router;
    };
}
