//
// Created by black on 25. 12. 6..
//

#pragma once
#include <core/command/CommandRouterBase.hpp>
#include <core/di/DIContainer.hpp>
#include <core/manager/SceneManager.hpp>
#include <platform/IWindow.hpp>
#include <core/thread/LogicThread.hpp>
#include <core/command/UICommandBus.hpp>

namespace rts::core {

    class GameApp {
    public:
        explicit GameApp(DIContainer& di);

        void run();
    private:
        DIContainer& m_di;
        std::shared_ptr<thread::LogicThread> m_logicThread;
        std::shared_ptr<platform::IWindow> m_window;
        std::shared_ptr<command::UICommandBus> m_uiBus;
        std::shared_ptr<command::UICommandRouter> m_uiRouter;
        std::shared_ptr<manager::SceneManager> m_sceneManager;
    };
} // namespace rts::core
