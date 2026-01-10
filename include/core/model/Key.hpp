//
// Created by black on 26. 1. 1..
//
#pragma once

namespace rts::core::model {
    enum class Key : unsigned short {
        // =================================================
        // 알파벳 (26)
        // =================================================
        A, B, C, D, E, F, G,
        H, I, J, K, L, M,
        N, O, P, Q, R, S,
        T, U, V, W, X, Y, Z,

        // =================================================
        // 숫자 (상단 숫자열 10)
        // =================================================
        Num0, Num1, Num2, Num3, Num4,
        Num5, Num6, Num7, Num8, Num9,

        // =================================================
        // 기호 키 (미국/ISO 공통 기준)
        // =================================================
        Backquote, // ` ~
        Minus, // - _
        Equal, // = +
        LeftBracket, // [ {
        RightBracket, // ] }
        Backslash, // \ |
        Semicolon, // ; :
        Apostrophe, // ' "
        Comma, // , <
        Period, // . >
        Slash, // / ?
        LBracket,
        RBracket,
        Grave,
        Hyphen,

        // =================================================
        // 기능 / 제어 키
        // =================================================
        Escape,
        LControl,
        LShift,
        LAlt,
        LSystem,
        RControl,
        RShift,
        RAlt,
        RSystem,
        Enter,
        Space,
        Tab,
        Backspace,
        CapsLock,
        Menu,

        // =================================================
        // 기능 키 (F1 ~ F12)
        // =================================================
        F1, F2, F3, F4,
        F5, F6, F7, F8,
        F9, F10, F11, F12,
        F13, F14, F15, F16,

        // =================================================
        // 방향키
        // =================================================
        Left,
        Right,
        Up,
        Down,

        // =================================================
        // 연산
        // =================================================
        Add,
        Subtract,
        Multiply,
        Divide,


        // =================================================
        // 편집 / 네비게이션
        // =================================================
        Insert,
        Delete,
        Home,
        End,
        PageUp,
        PageDown,

        // =================================================
        // Numpad (17)
        // =================================================
        Numpad0, Numpad1, Numpad2, Numpad3, Numpad4,
        Numpad5, Numpad6, Numpad7, Numpad8, Numpad9,

        NumpadAdd, // +
        NumpadSubtract, // -
        NumpadMultiply, // *
        NumpadDivide, // /
        NumpadDecimal, // .
        NumpadEnter,


        // =================================================
        // System
        // =================================================
        PrintScreen,
        ScrollLock,
        Pause,

        // =================================================
        // 상태
        // =================================================
        Unknown
    };

        enum class Scan : int16_t {
        Unknown = -1, //!< Unknown or unsupported scancode

        // --- Alphabet ---
        A = 0, B, C, D, E, F, G,
        H, I, J, K, L, M,
        N, O, P, Q, R, S,
        T, U, V, W, X, Y, Z,

        // --- Number row ---
        Num1, Num2, Num3, Num4, Num5,
        Num6, Num7, Num8, Num9, Num0,

        // --- Control ---
        Enter,
        Escape,
        Backspace,
        Tab,
        Space,

        // --- Symbols ---
        Hyphen,
        Equal,
        LBracket,
        RBracket,
        Backslash,
        Semicolon,
        Apostrophe,
        Grave,
        Comma,
        Period,
        Slash,

        // --- Function keys ---
        F1,  F2,  F3,  F4,  F5,  F6,
        F7,  F8,  F9,  F10, F11, F12,
        F13, F14, F15, F16, F17, F18,
        F19, F20, F21, F22, F23, F24,

        // --- Lock / system ---
        CapsLock,
        PrintScreen,
        ScrollLock,
        Pause,

        // --- Navigation ---
        Insert,
        Home,
        PageUp,
        Delete,
        End,
        PageDown,

        // --- Arrows ---
        Right,
        Left,
        Down,
        Up,

        // --- Numpad ---
        NumLock,
        NumpadDivide,
        NumpadMultiply,
        NumpadMinus,
        NumpadPlus,
        NumpadEqual,
        NumpadEnter,
        NumpadDecimal,
        Numpad1,
        Numpad2,
        Numpad3,
        Numpad4,
        Numpad5,
        Numpad6,
        Numpad7,
        Numpad8,
        Numpad9,
        Numpad0,

        // --- Non-US / special ---
        NonUsBackslash,
        Application,
        Execute,
        ModeChange,
        Help,
        Menu,
        Select,
        Redo,
        Undo,
        Cut,
        Copy,
        Paste,

        // --- Media ---
        VolumeMute,
        VolumeUp,
        VolumeDown,
        MediaPlayPause,
        MediaStop,
        MediaNextTrack,
        MediaPreviousTrack,

        // --- Modifiers (physical) ---
        LControl,
        LShift,
        LAlt,
        LSystem,
        RControl,
        RShift,
        RAlt,
        RSystem,

        // --- Browser / OS ---
        Back,
        Forward,
        Refresh,
        Stop,
        Search,
        Favorites,
        HomePage,

        // --- Launch ---
        LaunchApplication1,
        LaunchApplication2,
        LaunchMail,
        LaunchMediaSelect
    };

    enum class KeyModifier : uint8_t { None = 0, Shift = 1 << 0, Ctrl = 1 << 1, Alt = 1 << 2, System = 1 << 3, };

    enum class KeyAction : uint8_t {
        Press,
        Release,
        Repeat
    };

    inline KeyModifier operator|(KeyModifier a, KeyModifier b) {
        return static_cast<KeyModifier>(
            static_cast<uint8_t>(a) | static_cast<uint8_t>(b)
        );
    }

    inline KeyModifier operator&(KeyModifier a, KeyModifier b) {
        return static_cast<KeyModifier>(
            static_cast<uint8_t>(a) & static_cast<uint8_t>(b)
        );
    }

    inline bool has(KeyModifier m, KeyModifier f) {
        return (static_cast<uint8_t>(m) & static_cast<uint8_t>(f)) != 0;
    }


}
