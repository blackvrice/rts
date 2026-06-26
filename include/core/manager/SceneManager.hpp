//
// Created by black on 25. 12. 25..
//

#pragma once
#include <atomic>
#include <memory>

namespace rts::core {
    class DIContainer;
}

namespace rts::core::command {
    class LogicCommandBus;
    class UICommandBus;
}

namespace rts::core::scene {
    class IScene;
}

namespace rts::core::thread {
    class LogicThread;
}

namespace rts::core::manager {
    enum class SceneId {
        Login,
        Game,
        Lobby,
    };
    class SceneManager {
    public:
        SceneManager(core::DIContainer& di, command::LogicCommandBus& logicBus,
                     command::UICommandBus& uiBus, thread::LogicThread& logicThread);

        void changeScene(SceneId id);
        // Applies a scene change requested via SceneChangeCommand. Called on the main
        // thread (the command handler runs on the logic thread and only sets a flag),
        // so the actual swap / router reset never races a dispatch.
        void applyPendingScene();
        void update() const;
        void tick(float dt) const;
        void render() const;
        scene::IScene& getScene() const;

    private:
        // Registers the SceneChangeCommand handler on the shared logic router. Called
        // from the ctor and again after each scene change clears that router.
        void registerSceneChangeHandler();

        std::shared_ptr<scene::IScene> m_scene;
        core::DIContainer& di;
        command::LogicCommandBus& m_logicBus;
        command::UICommandBus& m_uiBus;
        thread::LogicThread& m_logicThread;
        std::atomic<int> m_pendingScene { -1 };  // SceneId requested but not yet applied
    };
}
