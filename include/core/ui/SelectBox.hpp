//
// Created by black on 26. 1. 4..
//


#pragma once

#include <core/ui/IUIElement.hpp>

#include "core/command/LogicCommandBus.hpp"
#include "core/font/FontMetrics.hpp"
#include "core/render/RenderQueue.hpp"

namespace rts::core::manager {
    class CameraManager;
}

namespace rts::core::ui {
    class SelectBox : public IUIElement {
    public:
        // additive/sameType are live modifier flags owned by the UI manager: shift
        // (add/toggle) and ctrl-or-double-click (select all of one type). They are
        // read when the drag/click is released.
        SelectBox(command::LogicCommandBus& bus, manager::CameraManager& camera,
                  const bool& additive, const bool& sameType);

        void update() override;

        void buildRenderCommands(render::RenderQueue& q) const override;

    private:
        bool m_visible  = false;
        model::Vector2D m_start;
        model::Vector2D m_end;
        const bool& m_additive;
        const bool& m_sameType;
    };
}
