//
// Created by black on 25. 12. 25..
//
#pragma once

#include <memory>

#include "GameUIManager.hpp"
#include "GameLogicManager.hpp"

namespace rts::scene {
    class GameScene : public IScene {
    public:
        GameScene(std::shared_ptr<manager::GameUIManager>, std::shared_ptr<manager::GameLogicManager>);

        void update() override;
        void render() override;
    };
}
