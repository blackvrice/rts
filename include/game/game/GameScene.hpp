//
// Created by black on 25. 12. 25..
//
#pragma once

#include <memory>
#include "core/scene/IScene.hpp"

namespace rts::core::world {
    class GameWorld;
}

namespace rts::core::scene {
    class GameScene : public IScene {
    public:
        GameScene(const std::shared_ptr<manager::IUIManager> &uiManager, const std::shared_ptr<manager::ILogicManager> &logicManager, const std::shared_ptr<core::world::GameWorld> &world);

        void update() override;
        void render() override;
        void tick(float dt) override;
    private:
        std::shared_ptr<core::world::GameWorld> world;
    };
}
