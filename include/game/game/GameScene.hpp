//
// Created by black on 25. 12. 25..
//
#pragma once

#include <memory>
#include "core/scene/IScene.hpp"
#include "GameUIManager.hpp"
#include "GameLogicManager.hpp"

namespace rts::scene {
    class GameScene : public IScene {
    public:
        GameScene(const std::shared_ptr<manager::IUIManager> &uiManager, const std::shared_ptr<manager::ILogicManager> &logicManager);

        void update() override;
        void render() override;
    };
}
