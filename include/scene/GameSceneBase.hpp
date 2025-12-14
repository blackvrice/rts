#pragma once
#include "Scene.hpp"
#include <memory>
#include "manager/GameManager.hpp"

namespace rts {

    class GameSceneBase : public Scene {
    public:
        GameSceneBase(std::shared_ptr<SceneManager> mgr,
                      std::shared_ptr<rts::manager::GameManager> game)
            : Scene(std::move(mgr)), m_game(std::move(game)) {}

    protected:
        std::shared_ptr<rts::manager::GameManager> m_game;
    };

} // namespace rts
