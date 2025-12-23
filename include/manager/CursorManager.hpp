//
// Created by black on 25. 12. 14..
//
//
// Created by black on 25. 12. 7..
//
// include/rts/manager/GameManager.hpp
#pragma once
#include <map>
#include <unordered_map>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Cursor.hpp>

namespace rts::manager {
    enum class CursorType {
        Default,
        Select,
        Move,
        Attack,
        Forbidden,
        Text
    };

    class CursorManager {
    public:
        void load();

        void apply(sf::RenderWindow& win, CursorType type);
    private:
        std::unordered_map<CursorType, sf::Cursor> m_cursors;

        void loadCursor(CursorType t, sf::Cursor::Type sys);

        void loadCursor(CursorType t, const std::string& file, sf::Vector2u hot);
    };


}
