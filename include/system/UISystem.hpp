//
// Created by black on 25. 12. 19..
//
// include/rts/system/UISystem.hpp
#pragma once
#include <vector>
#include <memory>
#include <SFML/Graphics/RenderWindow.hpp>

#include <ui/UIElement.hpp>
#include <manager/CursorManager.hpp>

namespace rts::system {

    class UISystem {
    public:
        explicit UISystem(rts::manager::CursorManager& cursor);

        void add(std::shared_ptr<rts::ui::UIElement> ui);

        void handleEvent(const sf::Event& ev);
        void update(sf::RenderWindow& window);

        void render(sf::RenderTarget& target);

        bool isUIBlocking() const;

    private:
        rts::manager::CursorManager& m_cursor;
        std::vector<std::shared_ptr<rts::ui::UIElement>> m_elements{};

        bool m_blocking = false;
    };

}
