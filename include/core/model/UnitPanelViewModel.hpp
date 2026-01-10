//
// Created by black on 26. 1. 6..
//

#pragma once
#include <memory>
#include <utility>

#include "IViewModel.hpp"
#include "State.hpp"

namespace rts::core::model {
    class UnitPanelViewModel : public IViewModel {
    public:
        UnitPanelViewModel(std::shared_ptr<GameState> state)
            : m_state(std::move(state)) {}

        void update() override {
            if (!m_state || m_state->selectedUnits().empty()) {
                m_visible = false;
                return;
            }

            m_visible = true;
            m_hp = m_state->selectedUnits()[0]->hp();
        }

        bool visible() const override { return m_visible; }
        void setVisible(bool v) override { m_visible = v; }

        const char* name() const override {
            return "UnitPanelViewModel";
        }

        bool expired() const override {
            return m_state == nullptr;
        }

        int hp() const { return m_hp; }

    private:
        std::shared_ptr<GameState> m_state;
        bool m_visible = true;
        int m_hp = 0;
    };
}
