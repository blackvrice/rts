//
// Created by black on 25. 12. 21..
//

#include "manager/GameUIManager.hpp"

namespace rts::manager {
    void GameUIManager::update() {
        for (const auto& element : m_elements) {
            element->update();
        }
    }

    void GameUIManager::render() {
        for (const auto& element : m_elements) {
            element->render();
        }
    }
}
