//
// Created by black on 25. 12. 23..
//

#pragma once
#include "manager/GameManager.hpp"
#include "scene/Scene.hpp"
#include "manager/GameUIManager.hpp"
#include "system/UISystem.hpp"
#include "ui/Button.hpp"
#include "ui/TextBox.hpp"
#include "ui/PasswordTextBox.hpp"


namespace rts::scene {

    // LobbyScene.hpp
    class LobbyScene : public Scene {
    public:
        LobbyScene();

        void handleEvent() override;
        void update() override;
        void render() override;

    private:
    };


} // namespace rts::scene
