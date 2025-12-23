//
// Created by black on 25. 12. 6..
//

#pragma once
#include "core/DIContainer.hpp"
#include "manager/IUIManager.hpp"

#include "platform/sfml/sfmlWindow.hpp"

namespace rts::core {

    class GameApp {
    public:
        explicit GameApp(DIContainer& di);

        void run();
    private:
        DIContainer& m_di;
        std::shared_ptr<platform::IWindow> m_window;
        std::shared_ptr<manager::IUIManager> m_uiManager;
        std::shared_ptr<command::CommandBus> m_uiBus;
        std::shared_ptr<command::CommandRouter> m_uiRouter;
    };
} // namespace rts::core
