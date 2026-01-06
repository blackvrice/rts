//
// Created by black on 25. 12. 22..
//

#pragma once
#include <core/model/Vector2D.hpp>
#include <core/command/UICommandBus.hpp>
#include "core/command/UICommandBus.hpp"

namespace rts::platform {
    class IWindow {
    public:
        explicit IWindow(command::UICommandBus& bus)
      : m_bus(bus) {}
        virtual ~IWindow() = default;

        virtual void clear() = 0;
        virtual bool isOpen() const = 0;
        virtual void pollEvents() = 0;
        virtual void display() = 0;
        virtual void* getNativeHandle() = 0;
    protected:
        command::UICommandBus& m_bus;
    };
}
