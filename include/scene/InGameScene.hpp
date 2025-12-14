//
// Created by black on 25. 12. 6..
//
// include/rts/scene/InGameScene.hpp
#pragma once

#include <memory>
#include "scene/Scene.hpp"
#include "manager/GameManager.hpp"
#include "presenter/GamePresenter.hpp"
#include "view/GameView.hpp"

namespace rts::scene {

    class InGameScene : public Scene {
    public:
        InGameScene(const std::shared_ptr<SceneManager> &mgr,
                    const std::shared_ptr<manager::GameManager> &game);

        void onEnter() override;
        void handleEvent(const sf::Event& ev) override;
        void update(float dt) override;
        void render(sf::RenderTarget& target) override;


    private:
        std::shared_ptr<manager::GameManager>   m_game;
        std::shared_ptr<presenter::GamePresenter> m_presenter;
        std::shared_ptr<view::GameView>           m_view;
    };

} // namespace rts::scene
