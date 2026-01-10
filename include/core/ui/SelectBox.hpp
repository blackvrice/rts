//
// Created by black on 26. 1. 4..
//


#pragma once

#include <core/ui/IUIElement.hpp>

#include "core/command/LogicCommandBus.hpp"
#include "core/font/FontMetrics.hpp"
#include "core/render/RenderQueue.hpp"

namespace rts::core::ui {
    class SelectBox : public IUIElement {
    public:
        explicit SelectBox(command::LogicCommandBus& bus);

        void update() override;

        void buildRenderCommands(render::RenderQueue& q) const override;

    private:
        bool m_visible  = false;
        model::Vector2D m_start;
        model::Vector2D m_end;
    };
}