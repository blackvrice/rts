//
// Created by black on 25. 12. 6..
//
// src/view/GameView.cpp
#include "view/GameView.hpp"

namespace rts::view {

    GameView::GameView()
    {
        m_circle.setRadius(8.f);
        m_circle.setOrigin({8.f, 8.f});
        m_circle.setFillColor(sf::Color::White);
    }

    void GameView::applySnapshot(const rts::model::Snapshot& snapshot)
    {
        m_units.clear();
        for (auto const& u : snapshot.units) {
            sf::CircleShape shape;
            shape.setPosition(u.position);
            shape.setRadius(u.radius);
            shape.setFillColor(u.color);
            m_units.push_back(shape);
        }
    }

    void GameView::render(sf::RenderTarget& target)
    {
        for (auto& shape : m_units)
            target.draw(shape);
    }

    void GameView::renderDragBox(sf::RenderTarget& target, const sf::FloatRect& rect)
    {
        if (rect.size.x <= 0.f || rect.size.y <= 0.f)
            return;  // 안전장치

        sf::RectangleShape shape;
        shape.setPosition({rect.position.x, rect.position.y});
        shape.setSize({rect.size.x, rect.size.y});
        shape.setFillColor(sf::Color(0, 255, 0, 50));
        shape.setOutlineColor(sf::Color::Green);
        shape.setOutlineThickness(1.f);

        target.draw(shape);
    }

} // namespace rts::view
