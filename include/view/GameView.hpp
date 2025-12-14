//
// Created by black on 25. 12. 6..
//

// include/rts/view/GameView.hpp
#pragma once
#include <SFML/Graphics.hpp>
#include "../model/Snapshot.hpp"

namespace rts::view {

    class GameView {
    public:
        GameView();

        void applySnapshot(const rts::model::Snapshot& snapshot);
        void render(sf::RenderTarget& target);
        void renderDragBox(sf::RenderTarget& target, const sf::FloatRect& rect);

    private:
        sf::CircleShape  m_circle;
        std::vector<sf::CircleShape> m_units;
    };

} // namespace rts::view
