//
// Created by black on 25. 12. 25..
//

#pragma once

#include <scene/IScene.hpp>

namespace rts::scene {
    class LobbyScene : public IScene {
    public:
        LobbyScene();
        void update() override;
        void render() override;
    private:

    };
}