//
// Created by black on 25. 12. 6..
//
#include "presenter/GamePresenter.hpp"
#include "core/Command.hpp"
#include "component/Velocity.hpp"
#include "component/Selection.hpp"
#include "component/Position.hpp"
#include "presenter/SelectCommand.hpp"
#include <SFML/Window/Mouse.hpp>
#include <cmath>

namespace rts::presenter {

    // ------------------------------
    // Move Command (GameManager 기반)
    // ------------------------------
    class MoveCommand final : public rts::core::Command {
    public:
        explicit MoveCommand(sf::Vector2f target)
            : m_target(target) {}

        void execute(rts::manager::GameManager& game) override
        {
            auto& reg = game.registry();

            auto& selStorage = reg.storage<rts::component::Selection>();
            auto& posStorage = reg.storage<rts::component::Position>();
            auto& velStorage = reg.storage<rts::component::Velocity>();

            for (auto& [e, sel] : selStorage.data()) {
                if (!sel.selected) continue;

                if (auto* pos = posStorage.get(e)) {

                    sf::Vector2f diff = m_target - pos->value;
                    float len2 = diff.x * diff.x + diff.y * diff.y;
                    if (len2 < 0.0001f) continue;

                    float len = std::sqrt(len2);
                    sf::Vector2f dir = diff / len;

                    rts::component::Velocity v;
                    v.value = dir * 100.f;  // 속도
                    v.target = m_target;    // ★ 중요

                    velStorage.add(e, v);
                }
            }
        }

    private:
        sf::Vector2f m_target;
    };

    // ------------------------------
    // GamePresenter
    // ------------------------------
    GamePresenter::GamePresenter(std::shared_ptr<manager::GameManager> game,
                                 std::shared_ptr<core::CommandBus> bus)
        : m_game(std::move(game)), m_bus(std::move(bus))
    {}

    sf::FloatRect GamePresenter::currentDragRect() const
    {
        float left   = std::min(m_dragStart.x, m_dragEnd.x);
        float top    = std::min(m_dragStart.y, m_dragEnd.y);
        float width  = std::abs(m_dragStart.x - m_dragEnd.x);
        float height = std::abs(m_dragStart.y - m_dragEnd.y);

        return sf::FloatRect({left, top}, {width, height});
    }

    void GamePresenter::handleEvent(const sf::Event& ev)
    {
        // --------------------------
        // Mouse Button Down
        // --------------------------
        if (ev.is<sf::Event::MouseButtonPressed>()) {

            auto mouseEvent = ev.getIf<sf::Event::MouseButtonPressed>();

            // Right-click → MoveCommand
            if (mouseEvent && mouseEvent->button == sf::Mouse::Button::Right)
            {
                sf::Vector2f target(
                    (float)mouseEvent->position.x,
                    (float)mouseEvent->position.y
                );

                auto cmd = std::make_shared<MoveCommand>(target);
                m_bus->push(cmd);
            }

            // Left-click → start drag box
            else if (mouseEvent && mouseEvent->button == sf::Mouse::Button::Left)
            {
                m_dragging  = true;
                m_dragStart = sf::Vector2f(
                    (float)mouseEvent->position.x,
                    (float)mouseEvent->position.y
                );
                m_dragEnd = m_dragStart;
            }
        }
        // --------------------------
        // Mouse Move (drag update)
        // --------------------------
        else if (ev.is<sf::Event::MouseMoved>())
        {
            if (m_dragging) {
                auto* mouse = ev.getIf<sf::Event::MouseMoved>();
                m_dragEnd = sf::Vector2f(
                    (float)mouse->position.x,
                    (float)mouse->position.y
                );
            }
        }
        // --------------------------
        // Mouse Button Up → SelectCommand
        // --------------------------
        else if (ev.is<sf::Event::MouseButtonReleased>())
        {
            auto mouseEvent = ev.getIf<sf::Event::MouseButtonReleased>();

            if (mouseEvent && mouseEvent->button == sf::Mouse::Button::Left)
            {
                if (m_dragging)
                {
                    m_dragging = false;

                    sf::Vector2f p1 = m_dragStart;
                    sf::Vector2f p2(
                        (float)mouseEvent->position.x,
                        (float)mouseEvent->position.y
                    );

                    sf::FloatRect rect(
                        { std::min(p1.x, p2.x), std::min(p1.y, p2.y) },
                        { std::abs(p1.x - p2.x), std::abs(p1.y - p2.y) }
                    );

                    auto cmd = std::make_shared<SelectCommand>(rect);
                    m_bus->push(cmd);
                }
            }
        }
    }

    // ------------------------------
    // Snapshot Builder
    // ------------------------------
    rts::model::Snapshot GamePresenter::buildSnapshot() const
    {
        return m_game->buildSnapshot();
    }

} // namespace rts::presenter
