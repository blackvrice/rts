//
// Created by black on 25. 12. 25..
//


#include "game/game/GameScene.hpp"

namespace rts::scene {
    GameScene::GameScene(const std::shared_ptr<manager::IUIManager> &uiManager,
                         const std::shared_ptr<manager::ILogicManager> &logicManager) : IScene(
        uiManager, logicManager) {
    }

    void GameScene::update() {
        // 1️⃣ Logic 업데이트
        m_logicManager->update();

        // 2️⃣ Logic → UI ViewModel 동기화 (🔥 핵심)
        m_uiManager->syncWithLogic();

        // 3️⃣ UI 업데이트 (ViewModel 기반)
        m_uiManager->update();
    }

    void GameScene::render() {
        // 4️⃣ 렌더링
        m_uiManager->render();
    }
}
