//
// Created by black on 25. 12. 22..
//

#pragma once
#include <../core/model/Vector2D.hpp>
#include <command/UICommandBus.hpp>

namespace rts::platform {
    class IWindow {
    public:
        explicit IWindow(command::UICommandBus& bus)
      : m_bus(bus) {}
        virtual ~IWindow() = default;

        virtual bool isOpen() const = 0;
        virtual void pollEvents() = 0;
        virtual void display() = 0;
    protected:
        command::UICommandBus& m_bus;
    };
}
