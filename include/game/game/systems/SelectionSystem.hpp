#pragma once

#include <memory>
#include <vector>

#include "core/model/Vector2D.hpp"

namespace rts::core::model {
    class IGameElement;
    class Rect;
}

namespace rts::core::world {
    class GameWorld;
}

namespace rts::core::manager {
    class SelectionSystem {
    public:
        using SelectedList = std::vector<std::weak_ptr<model::IGameElement>>;

        // Capped so huge drags don't select unbounded armies.
        static constexpr std::size_t kMaxSelection = 24;

        void clear();
        void selectElement(model::IGameElement& element);
        void addSelectedElement(model::IGameElement& element);
        // Drag-box or click selection. additive (shift) adds to / toggles the
        // current selection; otherwise it replaces. Drags prefer the player's
        // units (units outrank buildings/resources); a click picks the nearest
        // element under the cursor.
        void selectInArea(const world::GameWorld& world, const model::Rect& area, bool additive = false);
        // Selects every live unit sharing the type/team of the unit under the
        // point (ctrl-click / double-click). Falls back to a click if none.
        void selectSameType(const world::GameWorld& world, const model::Vector2D& point, bool additive = false);
        void replaceSelected(SelectedList selected);

        const SelectedList& selected() const noexcept;

    private:
        // Adds the chosen elements (capped). With additive, skips duplicates;
        // when toggle is also set, an already-selected element is deselected.
        void applySelection(const std::vector<std::shared_ptr<model::IGameElement>>& chosen,
                            bool additive, bool toggle);

        SelectedList m_selectedElements;
    };
}
