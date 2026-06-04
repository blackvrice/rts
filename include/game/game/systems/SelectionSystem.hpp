#pragma once

#include <memory>
#include <vector>

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

        void clear();
        void selectElement(model::IGameElement& element);
        void addSelectedElement(model::IGameElement& element);
        void selectInArea(const world::GameWorld& world, const model::Rect& area);
        void replaceSelected(SelectedList selected);

        const SelectedList& selected() const noexcept;

    private:
        SelectedList m_selectedElements;
    };
}
