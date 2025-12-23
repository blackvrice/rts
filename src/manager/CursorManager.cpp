//
// Created by black on 25. 12. 14..
//

#include "manager/CursorManager.hpp"

#include <SFML/Graphics/Image.hpp>

namespace rts::manager {
    void CursorManager::load() {
        loadCursor(CursorType::Default, sf::Cursor::Type::Arrow);

        // loadCursor(CursorType::Select,  "cursor_select.png", {16,16});
        // loadCursor(CursorType::Move,    "cursor_move.png",   {16,16});
        // loadCursor(CursorType::Attack,  "cursor_attack.png", {8,8});
        // loadCursor(CursorType::Forbidden,"cursor_block.png", {16,16});

        loadCursor(CursorType::Text, sf::Cursor::Type::Text);
    }

    void CursorManager::apply(sf::RenderWindow& win, CursorType type) {
        win.setMouseCursor(m_cursors[type]);
    }

    void CursorManager::loadCursor(CursorType t, sf::Cursor::Type sys) {
        auto c = sf::Cursor::createFromSystem(sys);
        m_cursors[t] = std::move(c.value());
    }

    void CursorManager::loadCursor(CursorType t, const std::string& file, sf::Vector2u hot) {
        sf::Image img(file);
        img.loadFromFile(file);

        auto c = sf::Cursor::createFromPixels(img.getPixelsPtr(), img.getSize(), hot);
        m_cursors[t] = std::move(c.value());
    }
}
