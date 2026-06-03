//
// Created by black on 25. 12. 6..
//

#pragma once
#include <memory>

namespace rts::core {
    class DIContainer;

    namespace command {
        class LogicCommandBus;
        class LogicCommandRouter;
        class UICommandBus;
        class UICommandRouter;
    }

    namespace manager {
        class SceneManager;
    }

    namespace render {
        class IRenderManager;
        class RenderContext;
        class RenderQueue;
    }

    namespace thread {
        class LogicThread;
    }

    class GameApp {
    public:
        explicit GameApp(DIContainer& di);

        void run() const;
    private:
        DIContainer& m_di;
        std::shared_ptr<thread::LogicThread> m_logicThread;
        std::shared_ptr<command::UICommandBus> m_uiBus;
        std::shared_ptr<command::UICommandRouter> m_uiRouter;
        std::shared_ptr<command::LogicCommandBus> m_logicBus;
        std::shared_ptr<command::LogicCommandRouter> m_logicRouter;
        std::shared_ptr<manager::SceneManager> m_sceneManager;
        std::shared_ptr<render::RenderQueue> m_renderQueue;
        std::shared_ptr<render::IRenderManager> m_renderManager;
        std::shared_ptr<render::RenderContext> m_renderContext;
    };
} // namespace rts::core
