//
// Created by black on 25. 12. 25..
//

#pragma once

#include <core/ui/IUIElement.hpp>

#include "core/font/FontMetrics.hpp"
#include "core/render/RenderQueue.hpp"

namespace rts::core::ui {
    class TextBox : public IUIElement {
    public:
        TextBox(core::model::Vector2D pos,
                core::model::Vector2D size,
                const core::font::FontMetrics* metrics);

        void update() override;


        void buildRenderCommands(core::render::RenderQueue& q) const override;

        void eraseLast();

        void onSubmit();

        void copySelectionToClipboard();
        void cutSelectionToClipboard();
        void setboardColor(int);
        void setBackgroundColor(int);
        void setTextColor(int);
        void setTextSize(int);


    private:
        void moveCursorLeft();
        void moveCursorRight();
        void moveCursorHome();
        void moveCursorEnd();
        void updateCursorLineColumn();
        void rebuildLines();
        float measureTextWidthUpToCursor() const;

        static std::string utf32ToUtf8(const std::u32string &input);
        model::Vector2D m_pos;
        model::Vector2D m_size;

        std::u32string m_text;
        size_t m_cursor = 0;
        std::vector<std::u32string> m_lines;
        size_t m_cursorLine = 0;
        size_t m_cursorColumn = 0;
        bool m_cursorVisible = true;
        clock_t m_lastBlinkTime = 0;
        static constexpr clock_t BlinkInterval = CLOCKS_PER_SEC / 2; // 1초
        const core::font::FontMetrics* m_fontMetrics = nullptr;

        font::FontId m_fontId = 0;
        int m_textSize = 10;
        uint32_t m_boardColor = 0x000000FF;
        uint32_t m_backgroundColor = 0xFFFFFFFF;
        uint32_t m_textColor = 0x000000FF;
        bool m_hovered = false;
        bool m_focused = false;

        const core::font::FontMetrics *m_metrics;

    };
}
