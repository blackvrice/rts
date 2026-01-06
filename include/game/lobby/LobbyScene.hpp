//
// Created by black on 25. 12. 25..
//

#pragma once

#include <core/scene/IScene.hpp>

namespace rts::scene {
    class LobbyScene : public IScene {
    public:
        LobbyScene(const std::shared_ptr<manager::IUIManager> &uiManager, const std::shared_ptr<manager::ILogicManager> &logicManager);
        void update() override;
        void render() override;
    private:

    };
}