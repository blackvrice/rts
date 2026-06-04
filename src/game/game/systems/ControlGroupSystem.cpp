#include "game/game/systems/ControlGroupSystem.hpp"

#include <algorithm>

namespace rts::core::manager {
    void ControlGroupSystem::assign(uint16_t groupId, const SelectedList& selected) {
        if (groupId >= kGroupCount) {
            return;
        }

        auto& group = m_groups[groupId];
        group = selected;
        eraseExpired(group);
    }

    void ControlGroupSystem::add(uint16_t groupId, const SelectedList& selected) {
        if (groupId >= kGroupCount) {
            return;
        }

        auto& group = m_groups[groupId];
        eraseExpired(group);
        group.reserve(group.size() + selected.size());

        for (const auto& weak : selected) {
            if (auto element = weak.lock(); element && !containsPtr(group, element)) {
                group.emplace_back(element);
            }
        }
    }

    ControlGroupSystem::SelectedList ControlGroupSystem::select(uint16_t groupId) {
        if (groupId >= kGroupCount) {
            return {};
        }

        auto& group = m_groups[groupId];
        eraseExpired(group);
        return group;
    }

    void ControlGroupSystem::eraseExpired(SelectedList& elements) {
        elements.erase(
            std::remove_if(elements.begin(), elements.end(),
                           [](const auto& weak) { return weak.expired(); }),
            elements.end());
    }

    bool ControlGroupSystem::containsPtr(
        const SelectedList& elements,
        const std::shared_ptr<model::IGameElement>& element) {
        const auto* raw = element.get();
        for (const auto& weak : elements) {
            if (auto existing = weak.lock(); existing && existing.get() == raw) {
                return true;
            }
        }

        return false;
    }
}
