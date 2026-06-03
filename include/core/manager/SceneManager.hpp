//
// Created by black on 25. 12. 25..
//

#pragma once
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

namespace rts::core::manager {
    enum class SceneId {
        Login,
        Game,
        Lobby,
    };
    class SceneManager {
    public:
        SceneManager(core::DIContainer& di, command::LogicCommandBus& logicBus, command::UICommandBus& uiBus);

        void changeScene(SceneId id);
        void update() const;
        void tick(float dt) const;
        void render() const;
        scene::IScene& getScene() const;

    private:
        std::shared_ptr<scene::IScene> m_scene;
        core::DIContainer& di;
        command::LogicCommandBus& m_logicBus;
        command::UICommandBus& m_uiBus;
    };
}
