#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace rts::core::model {
    class IGameElement;
}

namespace rts::core::manager {
    class ControlGroupSystem {
    public:
        using SelectedList = std::vector<std::weak_ptr<model::IGameElement>>;

        void assign(uint16_t groupId, const SelectedList& selected);
        void add(uint16_t groupId, const SelectedList& selected);
        SelectedList select(uint16_t groupId);

    private:
        static constexpr int kGroupCount = 10;

        static void eraseExpired(SelectedList& elements);
        static bool containsPtr(const SelectedList& elements,
                                const std::shared_ptr<model::IGameElement>& element);

        std::array<SelectedList, kGroupCount> m_groups;
    };
}
