//
// Created by black on 25. 12. 7..
//
#pragma once

#include <string>

#include "Signal.hpp"
#include "../model/Vector2D.hpp"
#include "core/model/Event.hpp"
#include "core/render/RenderQueue.hpp"

namespace rts::core::ui {
    class IUIElement {
    public:
        virtual ~IUIElement() = default;

        // --- Update / Render ---
        virtual void update() = 0;
        virtual void buildRenderCommands(core::render::RenderQueue& q) const = 0;

        // =====================================================
        // Internal input entry points (UIManager only)
        // =====================================================

        // --- Mouse ---
        void handleMouseMove(const core::model::Vector2D &pos) {
            MouseMove(pos);
        }

        void handleMouseEnter(const core::model::Vector2D &pos) {
            MouseEnter(pos);
            HoverChanged(true);
        }

        void handleMouseLeave(const core::model::Vector2D &pos) {
            MouseLeave(pos);
            HoverChanged(false);
        }

        void handleMouseDown(const core::model::Vector2D &pos) {
            MouseDown(pos);
        }

        void handleMouseUp(const core::model::Vector2D &pos) {
            MouseUp(pos);
            MouseClick(pos); // 기본 클릭 처리
            Clicked();
        }

        void handleMouseWheel(int delta) {
            MouseWheel(delta);
        }

        // --- Keyboard ---
        void handleKeyChar(char c) {
            KeyChar(c);
        }

        void handleKeyDown(core::model::KeyEvent key) {
            KeyDown(key);
        }

        void handleKeyUp(core::model::KeyEvent key) {
            KeyUp(key);
        }



        // --- Focus ---
        void handleFocusGained() {
            FocusGained();
        }

        void handleFocusLost() {
            FocusLost();
        }

        // --- State ---
        void handleEnableChanged(const bool enabled) {
            EnableChanged(enabled);
        }

        void handleVisibleChanged(const bool visible) {
            VisibleChanged(visible);
        }

        // --- Value / Action ---
        void handleTextChanged(const std::string &text) {
            TextChanged(text);
        }

        void handleSubmitted() {
            Submitted();
        }

        void handleCanceled() {
            Canceled();
        }

        void handleTextInput(const char32_t c) {
            TextInput(c);
        }

    public:
        // =====================================================
        // Signals (Events)
        // =====================================================

        // --- Mouse ---
        core::Signal<const core::model::Vector2D &> MouseMove;
        core::Signal<const core::model::Vector2D &> MouseEnter;
        core::Signal<const core::model::Vector2D &> MouseLeave;
        core::Signal<const core::model::Vector2D &> MouseDown;
        core::Signal<const core::model::Vector2D &> MouseUp;
        core::Signal<const core::model::Vector2D &> MouseClick;
        core::Signal<int> MouseWheel;

        // --- Keyboard ---
        core::Signal<char> KeyChar;
        core::Signal<core::model::KeyEvent> KeyDown;
        core::Signal<core::model::KeyEvent> KeyUp;

        // --- Focus ---
        core::Signal<> FocusGained;
        core::Signal<> FocusLost;

        // --- State ---
        core::Signal<bool> HoverChanged;
        core::Signal<bool> EnableChanged;
        core::Signal<bool> VisibleChanged;

        // --- Value / Action ---
        core::Signal<const std::string &> TextChanged;
        core::Signal<> Clicked;
        core::Signal<> Submitted;
        core::Signal<> Canceled;

        // -- Text --
        core::Signal<char32_t> TextInput;
    };
}
