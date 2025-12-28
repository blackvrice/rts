//
// Created by black on 25. 12. 25..
//

#include <ui/TextBox.hpp>

#include "render/RenderQueue.hpp"
#include "ui/UIRenderType.hpp"

namespace rts::ui {
    TextBox::TextBox(core::Vector2D pos, core::Vector2D size)
    : m_pos(pos), m_size(size)
    {
        // 1️⃣ Hover 처리
        MouseMove += [this](const core::Vector2D& p) {
            bool inside =
                p.x >= m_pos.x && p.x <= m_pos.x + m_size.x &&
                p.y >= m_pos.y && p.y <= m_pos.y + m_size.y;

            if (inside != m_hovered) {
                m_hovered = inside;
                HoverChanged.emit(m_hovered);
            }
        };

        // 2️⃣ Focus 처리
        MouseDown += [this](const core::Vector2D& p) {
            bool inside =
                p.x >= m_pos.x && p.x <= m_pos.x + m_size.x &&
                p.y >= m_pos.y && p.y <= m_pos.y + m_size.y;

            if (inside && !m_focused) {
                m_focused = true;
                FocusGained.emit();
            }
            else if (!inside && m_focused) {
                m_focused = false;
                FocusLost.emit();
            }
        };

        // 3️⃣ 키 입력 처리
        KeyChar += [this](char c) {
            if (!m_focused) return;

            if (c == '\b') { // Backspace
                if (m_cursor > 0) {
                    m_text.erase(m_cursor - 1, 1);
                    --m_cursor;
                    TextChanged.emit(m_text);
                }
                return;
            }

            if (c == '\r' || c == '\n') { // Enter
                Submitted.emit();
                return;
            }

            // 일반 문자
            m_text.insert(m_cursor, 1, c);
            ++m_cursor;
            TextChanged.emit(m_text);
        };
    }
    void TextBox::buildRenderCommands(render::RenderQueue& q) const {
        q.emplace(
            render::RenderLayer::UI,
            10,
            render::DrawRect{
                m_pos.x, m_pos.y,
                m_size.x, m_size.y,
                0xFF333333
            }
        );

        q.emplace(
            render::RenderLayer::UI,
            11,
            render::DrawText{
                m_pos.x + 5,
                m_pos.y + 5,
                0xFFFFFFFF,
                m_fontId,
                m_text.c_str()
            }
        );
    }
}
