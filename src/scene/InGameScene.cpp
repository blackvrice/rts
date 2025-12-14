//
// Created by black on 25. 12. 6..
//
// src/scene/InGameScene.cpp
#include "scene/InGameScene.hpp"
#include "scene/SceneManager.hpp"

namespace rts::scene {

    InGameScene::InGameScene(const std::shared_ptr<SceneManager> &mgr,
                             const std::shared_ptr<manager::GameManager> &game)
        : Scene(mgr, game),
          m_game(std::move(game))
    {
        m_view      = std::make_shared<rts::view::GameView>();
        m_presenter = std::make_shared<rts::presenter::GamePresenter>(
            m_game,
            m_game->commands()
        );
    }

    void InGameScene::onEnter()
    {
        // 필요하면 화면 전환 효과, 초기 UI 설정 등
    }

    void InGameScene::handleEvent(const sf::Event& ev)
    {
        m_presenter->handleEvent(ev);
    }

    void InGameScene::update(float /*dt*/)
    {
        // LogicThread 가 따로 GameManager 를 업데이트하므로
        // 여기서는 특별히 할 일 없음
    }

    void InGameScene::render(sf::RenderTarget& target)
    {
        auto snapshot = m_presenter->buildSnapshot();
        m_view->applySnapshot(snapshot);
        m_view->render(target);

        if (m_presenter->isDragging()) {
            m_view->renderDragBox(target, m_presenter->currentDragRect());
        }
    }

} // namespace rts::scene
