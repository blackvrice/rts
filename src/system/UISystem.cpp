// src/system/UISystem.cpp
#include <system/UISystem.hpp>

#include "manager/CursorManager.hpp"
#include "ui/UIElement.hpp"

using namespace rts;
using namespace rts::system;

UISystem::UISystem(manager::CursorManager& cursor)
    : m_cursor(cursor) {}

void UISystem::add(std::shared_ptr<ui::UIElement> ui) {
    m_elements.push_back(std::move(ui));
}

void UISystem::handleEvent(const sf::Event& ev) {
    m_blocking = false;

    // 뒤에서부터 (위에 있는 UI 우선)
    for (auto it = m_elements.rbegin(); it != m_elements.rend(); ++it) {
        if ((*it)->handleEvent(ev)) {
            if ((*it)->blocksWorldInput())
                m_blocking = true;
            break;
        }
    }
}

void UISystem::update(sf::RenderWindow& window) {
    const auto mouse =
        window.mapPixelToCoords(sf::Mouse::getPosition(window));

    manager::CursorType cursorType = manager::CursorType::Default;
    m_blocking = false;

    for (auto it = m_elements.rbegin(); it != m_elements.rend(); ++it) {
        auto& ui = *it;
        ui->updateHover(mouse);

        if (ui->isHovered()) {
            if (ui->wantsTextCursor())
                cursorType = manager::CursorType::Text;

            if (ui->blocksWorldInput())
                m_blocking = true;

            break;
        }
    }

    m_cursor.apply(window, cursorType);
}

void UISystem::render(sf::RenderTarget& target) {
    for (auto& ui : m_elements)
        ui->render(target);
}

bool UISystem::isUIBlocking() const {
    return m_blocking;
}
