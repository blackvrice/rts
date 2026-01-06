//
// Created by black on 25. 12. 25..
//


#include <string>
#include <cstdint>
#include <codecvt>
#include <core/ui/TextBox.hpp>

#include "core/font/FontManager.hpp"
#include "core/model/Vector2D.hpp"
#include "core/render/RenderQueue.hpp"
#include "core/ui/UIRenderType.hpp"

namespace rts::core::ui {
    TextBox::TextBox(core::model::Vector2D pos, core::model::Vector2D size, const core::font::FontMetrics *metrics)
        : m_pos(pos), m_size(size), m_metrics(metrics) {
        rebuildLines();
        updateCursorLineColumn();
        // 1️⃣ Hover 처리
        MouseMove += [this](const core::model::Vector2D &p) {
            bool inside =
                    p.x >= m_pos.x && p.x <= m_pos.x + m_size.x &&
                    p.y >= m_pos.y && p.y <= m_pos.y + m_size.y;

            if (inside != m_hovered) {
                m_hovered = inside;
                HoverChanged(m_hovered);
            }
        };

        // 2️⃣ Focus 처리
        MouseDown += [this](const core::model::Vector2D &p) {
            bool inside =
                    p.x >= m_pos.x && p.x <= m_pos.x + m_size.x &&
                    p.y >= m_pos.y && p.y <= m_pos.y + m_size.y;

            if (inside && !m_focused) {
                m_focused = true;
                FocusGained();
            } else if (!inside && m_focused) {
                m_focused = false;
                FocusLost();
            }
        };

        // 3️⃣ 키 입력 처리
        KeyDown += [this](const core::model::KeyEvent &e) {
            if (!m_focused)
                return;

            using Key = core::model::Key;
            using Mod = core::model::KeyModifier;

            switch (e.key) {
                case Key::Left:
                    moveCursorLeft();
                    break;

                case Key::Right:
                    moveCursorRight();
                    break;

                case Key::Up:
                    moveCursorHome();
                    break;

                case Key::Down:
                    moveCursorEnd();
                    break;

                case Key::Backspace:
                    eraseLast();
                    break;

                case Key::Enter:
                    onSubmit();
                    break;

                case Key::C:
                    if (static_cast<bool>(e.mods & Mod::Ctrl))
                        copySelectionToClipboard();
                    break;

                case Key::X:
                    if (static_cast<bool>(e.mods & Mod::Ctrl))
                        cutSelectionToClipboard();
                    break;

                default:
                    break;
            }
        };

        TextInput += [this](char32_t ch) {
            if (!m_focused || ch < 32)
                return;

            m_text.insert(m_text.begin() + m_cursor, ch);
            ++m_cursor;
            rebuildLines();
            updateCursorLineColumn();
        };
    }

    void TextBox::update() {
        if (!m_focused) {
            m_cursorVisible = false;
            return;
        }

        const clock_t now = clock();

        if (now - m_lastBlinkTime >= BlinkInterval) {
            m_cursorVisible = !m_cursorVisible;
            m_lastBlinkTime = now;
        }
    }

    void TextBox::buildRenderCommands(core::render::RenderQueue &q) const {
        q.emplace(
            core::render::RenderLayer::UI,
            10,
            core::render::DrawRect{
                model::Rect(model::Vector2D{m_pos.x, m_pos.y},
                            model::Vector2D{m_size.x, m_size.y}),
                m_boardColor,
                m_backgroundColor
            }
        );

        if (m_focused && m_cursorVisible) {
            float cursorX =
                    m_pos.x + 5 + measureTextWidthUpToCursor();

            float cursorY = m_pos.y + 5;

            q.emplace(
                core::render::RenderLayer::UI,
                12,
                core::render::DrawRect{
                    model::Rect(model::Vector2D{cursorX, cursorY},
                                model::Vector2D{2.0f, static_cast<float>(m_textSize)}),
                    m_textColor,
                    m_textColor
                }
            );
        }

        const std::string utf8Text = utf32ToUtf8(m_text);
        q.emplace(
            core::render::RenderLayer::UI,
            11,
            core::render::DrawText{
                m_pos + model::Vector2D{2.f, 2.f},
                m_textColor,
                m_fontId,
                m_textSize,
                utf8Text
            }
        );
    }

    void TextBox::copySelectionToClipboard() {
    }

    void TextBox::cutSelectionToClipboard() {
    }

    void TextBox::onSubmit() {
    }

    void TextBox::eraseLast() {
        if (m_cursor == 0 || m_text.empty())
            return;

        m_text.erase(m_text.begin() + (m_cursor - 1));
        --m_cursor;
        rebuildLines();
        updateCursorLineColumn();
    }

    void TextBox::setBackgroundColor(int color) {
        m_backgroundColor = color;
    }

    void TextBox::setboardColor(int color) {
        m_boardColor = color;
    }

    void TextBox::setTextColor(int color) {
        m_textColor = color;
    }

    void TextBox::setTextSize(int size) {
        m_textSize = size;
    }


    std::string TextBox::utf32ToUtf8(const std::u32string &input) {
        std::string output;
        output.reserve(input.size() * 4);

        for (char32_t ch: input) {
            if (ch <= 0x7F) {
                // 1-byte
                output.push_back(static_cast<char>(ch));
            } else if (ch <= 0x7FF) {
                // 2-byte
                output.push_back(static_cast<char>(0xC0 | (ch >> 6)));
                output.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
            } else if (ch <= 0xFFFF) {
                // 3-byte
                output.push_back(static_cast<char>(0xE0 | (ch >> 12)));
                output.push_back(static_cast<char>(0x80 | ((ch >> 6) & 0x3F)));
                output.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
            } else if (ch <= 0x10FFFF) {
                // 4-byte
                output.push_back(static_cast<char>(0xF0 | (ch >> 18)));
                output.push_back(static_cast<char>(0x80 | ((ch >> 12) & 0x3F)));
                output.push_back(static_cast<char>(0x80 | ((ch >> 6) & 0x3F)));
                output.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
            }
            // else: invalid Unicode → skip or replace
        }

        return output;
    }

    float TextBox::measureTextWidthUpToCursor() const {
        if (m_lines.empty())
            return 0.f;

        if (m_cursorLine >= m_lines.size())
            return 0.f;

        const auto &fm = core::font::FontManager::instance();
        const auto &metrics = fm.metrics();

        const auto &line = m_lines[m_cursorLine];

        float x = 0.f;
        char32_t prev = 0;

        for (size_t i = 0; i < m_cursorColumn && i < line.size(); ++i) {
            char32_t ch = line[i];
            x += metrics.kerning(prev, ch, m_fontId, m_textSize);
            x += metrics.advance(ch, m_fontId, m_textSize);
            prev = ch;
        }

        return x;
    }


    void TextBox::updateCursorLineColumn() {
        m_cursorLine = 0;
        m_cursorColumn = 0;

        size_t idx = 0;

        for (size_t line = 0; line < m_lines.size(); ++line) {
            const auto &l = m_lines[line];

            if (idx + l.size() >= m_cursor) {
                m_cursorLine = line;
                m_cursorColumn = m_cursor - idx;
                return;
            }

            idx += l.size() + 1; // +1 = '\n'
        }

        // fallback (맨 끝)
        if (!m_lines.empty()) {
            m_cursorLine = m_lines.size() - 1;
            m_cursorColumn = m_lines.back().size();
        }
    }


    void TextBox::moveCursorLeft() {
        if (m_cursor > 0) {
            --m_cursor;
            updateCursorLineColumn();
        }
    }

    void TextBox::moveCursorRight() {
        if (m_cursor < m_text.size()) {
            ++m_cursor;
            updateCursorLineColumn();
        }
    }

    void TextBox::moveCursorHome() {
        m_cursor = 0;
        updateCursorLineColumn();
    }

    void TextBox::moveCursorEnd() {
        m_cursor = m_text.size();
        updateCursorLineColumn();
    }

    void TextBox::rebuildLines() {
        m_lines.clear();
        m_lines.emplace_back();

        for (char32_t ch: m_text) {
            if (ch == U'\n')
                m_lines.emplace_back();
            else
                m_lines.back().push_back(ch);
        }
    }
}
