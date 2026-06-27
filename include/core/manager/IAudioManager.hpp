//
// Created by black on 26. 6. 27..
//

#pragma once


namespace rts::core::command {
    class AudioCommandBus;
    class AudioCommandRouter;
}

namespace rts::core::manager {

    class IAudioManager{
    public:
        explicit IAudioManager(command::AudioCommandBus &bus, command::AudioCommandRouter &router)
            : m_bus(bus), m_router(router) {
        }

        virtual ~IAudioManager() = default;

        virtual void update() = 0;

        virtual void tick(float dt) = 0;

    protected:
        command::AudioCommandBus &m_bus;
        command::AudioCommandRouter &m_router;
    };
}