//
// Created by black on 25. 12. 7..
//
#pragma once
#include "ui/UIElement.hpp"

namespace rts::manager {
    class UIManager {
    public:
        void add(std::shared_ptr<ui::UIElement> ui) {
            m_elements.push_back(ui);
        }

        void update(float dt);

        void render(sf::RenderTarget& target);

        bool handleEvent(const sf::Event& ev);

    private:
        std::vector<std::shared_ptr<ui::UIElement>> m_elements;
    };

    inline void UIManager::update(float dt) {
        for (auto& e : m_elements)
            e->update(dt);
    }

    inline void UIManager::render(sf::RenderTarget &target) {
        for (auto& e : m_elements)
            e->render(target);
    }

    inline bool UIManager::handleEvent(const sf::Event &ev) {
        for (auto& e : m_elements)
            if (e->handleEvent(ev))
                return true;  // UI가 이벤트를 소비하면 게임 입력 차단
        return false;
    }
}
