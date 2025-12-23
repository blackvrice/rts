//
// Created by black on 25. 12. 22..
//

#pragma once
#include <memory>
#include <command/CommandBus.hpp>

#include <command/CommandRouter.hpp>

#include "core/Vector2D.hpp"

namespace rts::platform {
    class IWindow {
    public:
        IWindow(const std::shared_ptr<command::CommandBus> &bus, const std::shared_ptr<command::CommandRouter> &router) : m_bus(bus), m_router(router) {};
        virtual ~IWindow() = default;

        virtual bool isOpen() const = 0;
        virtual void pollEvents() = 0;
        virtual void display() = 0;
        virtual core::Vector2D getSize() const = 0;
    protected:
        std::shared_ptr<command::CommandBus> m_bus;
        std::shared_ptr<command::CommandRouter> m_router;
    };
}
