#include "game/game/systems/SelectionSystem.hpp"

#include <core/model/IGameElement.hpp>
#include <core/model/Rect.hpp>
#include <core/world/GameWorld.hpp>

namespace rts::core::manager {
    void SelectionSystem::clear() {
        m_selectedElements.clear();
    }

    void SelectionSystem::selectElement(model::IGameElement& element) {
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

            if (auto gameElement = std::dynamic_pointer_cast<model::IGameElement>(element)) {
                selectElement(*gameElement);
            }
        }
    }

    void SelectionSystem::replaceSelected(SelectedList selected) {
        m_selectedElements = std::move(selected);
    }

    const SelectionSystem::SelectedList& SelectionSystem::selected() const noexcept {
        return m_selectedElements;
    }
}
