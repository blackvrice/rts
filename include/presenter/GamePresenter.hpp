//
// Created by black on 25. 12. 6..
//
// include/rts/presenter/GamePresenter.hpp
#pragma once

#include <memory>
#include <SFML/Window/Event.hpp>
#include <SFML/Graphics/Rect.hpp>
#include "model/Snapshot.hpp"
#include "../command/CommandBus.hpp"
#include "manager/GameManager.hpp"

namespace rts::presenter {

    class GamePresenter {
    public:
        GamePresenter(std::shared_ptr<rts::manager::GameManager> game,
                      std::shared_ptr<rts::thread::CommandBus> bus);

        void handleEvent(const sf::Event& ev);
        rts::model::Snapshot buildSnapshot() const;

        bool isDragging() const { return m_dragging; }
        sf::FloatRect currentDragRect() const;

    private:
        std::shared_ptr<rts::manager::GameManager> m_game;
        std::shared_ptr<rts::thread::CommandBus>    m_bus;

        bool m_dragging{false};
        sf::Vector2f m_dragStart{};
        sf::Vector2f m_dragEnd{};
    };

} // namespace rts::presenter
