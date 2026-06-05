#include "game/game/systems/SelectionSystem.hpp"

#include <core/model/IGameElement.hpp>
#include <core/model/Rect.hpp>
#include <core/world/GameWorld.hpp>

namespace rts::core::manager {
    void SelectionSystem::clear() {
        for (const auto& weak : m_selectedElements) {
            if (auto element = weak.lock()) {
                element->setSelected(false);
            }
        }

        m_selectedElements.clear();
    }

    void SelectionSystem::selectElement(model::IGameElement& element) {
        element.setSelected(true);
        m_selectedElements.push_back(element.weak_from_this());
    }

    void SelectionSystem::addSelectedElement(model::IGameElement& element) {
        selectElement(element);
    }

    void SelectionSystem::selectInArea(const world::GameWorld& world, const model::Rect& area) {
        clear();

        for (const auto& element : world.getElements()) {
            if (!area.contains(element->getPosition())) {
                continue;
            }

            if (auto gameElement = std::dynamic_pointer_cast<model::IGameElement>(element);
                gameElement && gameElement->getAction() != model::ActionType::Dead) {
                selectElement(*gameElement);
            }
        }
    }

    void SelectionSystem::replaceSelected(SelectedList selected) {
        clear();

        m_selectedElements = std::move(selected);
        for (const auto& weak : m_selectedElements) {
            if (auto element = weak.lock()) {
                element->setSelected(true);
            }
        }
    }

    const SelectionSystem::SelectedList& SelectionSystem::selected() const noexcept {
        return m_selectedElements;
    }
}
