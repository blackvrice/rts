//
// Created by black on 25. 12. 25..
//


#include "game/game/GameScene.hpp"

#include "core/manager/ILogicManager.hpp"
#include "core/manager/IUIManager.hpp"
#include "core/world/GameWorld.hpp"

namespace rts::core::scene {
    GameScene::GameScene(const std::shared_ptr<manager::IUIManager> &uiManager,
                         const std::shared_ptr<manager::ILogicManager> &logicManager,
                         const std::shared_ptr<core::world::GameWorld> &world
                         ) : IScene(
            uiManager, logicManager), world(world) {
    }

    void GameScene::update() {
        // 1️⃣ Logic 업데이트
        m_logicManager->update();

        auto lock = world->acquireReadLock();

        // 2️⃣ Logic → UI ViewModel 동기화 (🔥 핵심)
        m_uiManager->syncWithWorld();

        // 3️⃣ UI 업데이트 (ViewModel 기반)
        m_uiManager->update();
    }

    void GameScene::render() {
        auto lock = world->acquireReadLock();

        // 4️⃣ 렌더링
        m_uiManager->render();
    }

    void GameScene::tick(float dt) {
        m_logicManager->tick(dt);
    }
}
