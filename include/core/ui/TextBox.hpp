//
// Created by black on 25. 12. 25..
//

#pragma once

#include <ui/IUIElement.hpp>

namespace rts::ui {
    class TextBox : public IUIElement {
    public:
        TextBox(core::Vector2D pos,
                core::Vector2D size);

        void update() override;
        void buildRenderCommands(render::RenderQueue& q) const override;

    private:
        core::Vector2D m_pos;
        core::Vector2D m_size;

        std::string m_text;
        size_t m_cursor = 0;

        int m_fontId = 0;
        bool m_hovered = false;
        bool m_focused = false;
    };
}