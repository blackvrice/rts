//
// Created by black on 25. 12. 22..
//

#pragma once

#include <manager/IManager.hpp>

namespace rts::manager {
    class LoginUIManager : IManager {
    public:
        LoginUIManager(std::shared_ptr<command::CommandBus> bus, command::CommandRouter &router);

        void add(std::shared_ptr<ui::UIElement> ui) {
            m_elements.push_back(ui);
        }

        void update();

        void render();

        bool handleEvent();

    private:
        bool m_leftMouseDown = false;
        bool m_rightMouseDown = false;
        sf::Vector2i m_firstPos;
        sf::Vector2i m_secondPos;
        std::vector<std::shared_ptr<ui::UIElement> > m_elements;
    };
}
