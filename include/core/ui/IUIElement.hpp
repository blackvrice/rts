//
// Created by black on 25. 12. 7..
//
#pragma once

#include <string>

#include "command/UIRenderCommand.hpp"
#include "Signal.hpp"
#include "../model/Vector2D.hpp"
#include "render/RenderQueue.hpp"

namespace rts::ui {
    class IUIElement {
    public:
        virtual ~IUIElement() = default;

        // --- Update / Render ---
        virtual void update() = 0;
        virtual void buildRenderCommands(render::RenderQueue& q) const = 0;

        // =====================================================
        // Internal input entry points (UIManager only)
        // =====================================================

        // --- Mouse ---
        void handleMouseMove(const core::Vector2D &pos) {
            MouseMove.emit(pos);
        }

        void handleMouseEnter(const core::Vector2D &pos) {
            MouseEnter.emit(pos);
            HoverChanged.emit(true);
        }

        void handleMouseLeave(const core::Vector2D &pos) {
            MouseLeave.emit(pos);
            HoverChanged.emit(false);
        }

        void handleMouseDown(const core::Vector2D &pos) {
            MouseDown.emit(pos);
        }

        void handleMouseUp(const core::Vector2D &pos) {
            MouseUp.emit(pos);
            MouseClick.emit(pos); // 기본 클릭 처리
            Clicked.emit();
        }

        void handleMouseWheel(int delta) {
            MouseWheel.emit(delta);
        }

        // --- Keyboard ---
        void handleKeyChar(char c) {
            KeyChar.emit(c);
        }

        void handleKeyDown(int key) {
            KeyDown.emit(key);
        }

        void handleKeyUp(int key) {
            KeyUp.emit(key);
        }

        // --- Focus ---
        void handleFocusGained() {
            FocusGained.emit();
        }

        void handleFocusLost() {
            FocusLost.emit();
        }

        // --- State ---
        void handleEnableChanged(bool enabled) {
            EnableChanged.emit(enabled);
        }

        void handleVisibleChanged(bool visible) {
            VisibleChanged.emit(visible);
        }

        // --- Value / Action ---
        void handleTextChanged(const std::string &text) {
            TextChanged.emit(text);
        }

        void handleSubmitted() {
            Submitted.emit();
        }

        void handleCanceled() {
            Canceled.emit();
        }

    public:
        // =====================================================
        // Signals (Events)
        // =====================================================

        // --- Mouse ---
        core::Signal<const core::Vector2D &> MouseMove;
        core::Signal<const core::Vector2D &> MouseEnter;
        core::Signal<const core::Vector2D &> MouseLeave;
        core::Signal<const core::Vector2D &> MouseDown;
        core::Signal<const core::Vector2D &> MouseUp;
        core::Signal<const core::Vector2D &> MouseClick;
        core::Signal<int> MouseWheel;

        // --- Keyboard ---
        core::Signal<char> KeyChar;
        core::Signal<int> KeyDown;
        core::Signal<int> KeyUp;

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
    };
}
