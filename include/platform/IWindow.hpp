//
// Created by black on 25. 12. 22..
//

#pragma once
namespace rts::core::command {
    class UICommandBus;
}

namespace rts::platform {
    class IWindow {
    public:
        explicit IWindow(core::command::UICommandBus& bus)
      : m_bus(bus) {}
        virtual ~IWindow() = default;

        virtual void clear() = 0;
        virtual bool isOpen() const = 0;
        virtual void pollEvents() = 0;
        virtual void display() = 0;
        virtual void* getNativeHandle() = 0;
    protected:
        core::command::UICommandBus& m_bus;
    };
}
