//
// Created by black on 25. 12. 22..
//

#include <thread>
#include <platform/sfml/SfmlWindow.hpp>

#include "core/command/Command.hpp"
#include "core/command/LogicCommandBus.hpp"
#include "core/command/UICommandBus.hpp"
#include "core/font/FontManager.hpp"
#include "platform/sfml/SfmlFontMetrics.hpp"


namespace rts::platform::sfml {
    SfmlWindow::SfmlWindow(command::UICommandBus &bus)
        : IWindow(bus),
          m_window(sf::VideoMode({1920, 1080}), "RTS") {
        auto &fm = core::font::FontManager::instance();
        fm.setMetricsProvider(
            std::make_unique<SfmlFontMetrics>()
        );

        auto font = std::make_shared<sf::Font>("C:/Windows/Fonts/Arial.ttf");
        core::font::FontId uiFont = fm.registerFont("UI", font);
    }

    bool SfmlWindow::isOpen() const {
        return m_window.isOpen();
    }

    void SfmlWindow::pollEvents() {
        while (auto ev = m_window.pollEvent()) {
            if (!ev.has_value())
                continue;

            const auto &event = ev.value();

            if (event.is<sf::Event::MouseButtonPressed>()) {
                const auto *mouse = event.getIf<sf::Event::MouseButtonPressed>();
                auto position = core::model::Vector2D(mouse->position.x, mouse->position.y);
                if (mouse->button == sf::Mouse::Button::Left) {
                    m_bus.push(
                        std::make_unique<command::MouseLeftPressedCommand>(position)
                    );
                } else if (mouse->button == sf::Mouse::Button::Right) {
                    m_bus.push(
                        std::make_unique<command::MouseRightPressedCommand>(position)
                    );
                }
            }

            if (event.is<sf::Event::MouseButtonReleased>()) {
                const auto *mouse = event.getIf<sf::Event::MouseButtonReleased>();
                auto position = core::model::Vector2D(mouse->position.x, mouse->position.y);
                if (mouse->button == sf::Mouse::Button::Left) {
                    m_bus.push(
                        std::make_unique<command::MouseLeftReleasedCommand>(position)
                    );
                } else if (mouse->button == sf::Mouse::Button::Right) {
                    m_bus.push(
                        std::make_unique<command::MouseRightReleasedCommand>(position)
                    );
                }
            }

            if (event.is<sf::Event::MouseMoved>()) {
                const auto *mouse = event.getIf<sf::Event::MouseMoved>();
                auto position = core::model::Vector2D(mouse->position.x, mouse->position.y);
                m_bus.push(
                    std::make_unique<command::MouseMoveCommand>(position)
                );
            }

            if (event.is<sf::Event::KeyPressed>()) {
                const auto *key = event.getIf<sf::Event::KeyPressed>();
                using core::model::KeyModifier;
                using core::model::KeyAction;

                KeyModifier mods = KeyModifier::None;

                if (key->shift) mods = mods | KeyModifier::Shift;
                if (key->control) mods = mods | KeyModifier::Ctrl;
                if (key->alt) mods = mods | KeyModifier::Alt;
                if (key->system) mods = mods | KeyModifier::System;

                m_bus.push(
                    std::make_unique<command::KeyPressedCommand>(toKey(key->code), toScan(key->scancode), mods)
                );
            }

            if (event.is<sf::Event::TextEntered>()) {
                if (const auto *text = event.getIf<sf::Event::TextEntered>()) {
                    char32_t ch = text->unicode;

                    // 제어 문자 필터링 (엔터/백스페이스 등은 KeyPressed에서 처리)
                    if (ch >= 32) {
                        m_bus.push(
                            std::make_unique<command::TextEnteredCommand>(ch)
                        );
                    }
                }
            }

            if (event.is<sf::Event::Closed>()) {
                m_window.close();
            }
        }
    }

    void SfmlWindow::clear() {
        m_window.clear();
    }


    void SfmlWindow::display() {
        m_window.display();
    }

    void SfmlWindow::draw(const sf::Shape &shape) {
        m_window.draw(shape);
    }

    core::model::Key SfmlWindow::toKey(const sf::Keyboard::Key k) {
        using K = core::model::Key;
        using S = sf::Keyboard::Key;

        switch (k) {
            // --- 알파벳 ---
            case S::A: return K::A;
            case S::B: return K::B;
            case S::C: return K::C;
            case S::D: return K::D;
            case S::E: return K::E;
            case S::F: return K::F;
            case S::G: return K::G;
            case S::H: return K::H;
            case S::I: return K::I;
            case S::J: return K::J;
            case S::K: return K::K;
            case S::L: return K::L;
            case S::M: return K::M;
            case S::N: return K::N;
            case S::O: return K::O;
            case S::P: return K::P;
            case S::Q: return K::Q;
            case S::R: return K::R;
            case S::S: return K::S;
            case S::T: return K::T;
            case S::U: return K::U;
            case S::V: return K::V;
            case S::W: return K::W;
            case S::X: return K::X;
            case S::Y: return K::Y;
            case S::Z: return K::Z;

            // --- 숫자 (상단) ---
            case S::Num0: return K::Num0;
            case S::Num1: return K::Num1;
            case S::Num2: return K::Num2;
            case S::Num3: return K::Num3;
            case S::Num4: return K::Num4;
            case S::Num5: return K::Num5;
            case S::Num6: return K::Num6;
            case S::Num7: return K::Num7;
            case S::Num8: return K::Num8;
            case S::Num9: return K::Num9;

            // --- 제어 / 시스템 ---
            case S::Escape: return K::Escape;
            case S::LControl: return K::LControl;
            case S::LShift: return K::LShift;
            case S::LAlt: return K::LAlt;
            case S::LSystem: return K::LSystem;
            case S::RControl: return K::RControl;
            case S::RShift: return K::RShift;
            case S::RAlt: return K::RAlt;
            case S::RSystem: return K::RSystem;
            case S::Menu: return K::Menu;

            // --- 기호 ---
            case S::LBracket: return K::LBracket;
            case S::RBracket: return K::RBracket;
            case S::Semicolon: return K::Semicolon;
            case S::Comma: return K::Comma;
            case S::Period: return K::Period;
            case S::Apostrophe: return K::Apostrophe;
            case S::Slash: return K::Slash;
            case S::Backslash: return K::Backslash;
            case S::Grave: return K::Grave;
            case S::Equal: return K::Equal;
            case S::Hyphen: return K::Hyphen;

            // --- 공백 / 편집 ---
            case S::Space: return K::Space;
            case S::Enter: return K::Enter;
            case S::Backspace: return K::Backspace;
            case S::Tab: return K::Tab;

            // --- 네비게이션 ---
            case S::PageUp: return K::PageUp;
            case S::PageDown: return K::PageDown;
            case S::End: return K::End;
            case S::Home: return K::Home;
            case S::Insert: return K::Insert;
            case S::Delete: return K::Delete;

            // --- 연산 ---
            case S::Add: return K::Add;
            case S::Subtract: return K::Subtract;
            case S::Multiply: return K::Multiply;
            case S::Divide: return K::Divide;

            // --- 방향 ---
            case S::Left: return K::Left;
            case S::Right: return K::Right;
            case S::Up: return K::Up;
            case S::Down: return K::Down;

            // --- 넘패드 ---
            case S::Numpad0: return K::Numpad0;
            case S::Numpad1: return K::Numpad1;
            case S::Numpad2: return K::Numpad2;
            case S::Numpad3: return K::Numpad3;
            case S::Numpad4: return K::Numpad4;
            case S::Numpad5: return K::Numpad5;
            case S::Numpad6: return K::Numpad6;
            case S::Numpad7: return K::Numpad7;
            case S::Numpad8: return K::Numpad8;
            case S::Numpad9: return K::Numpad9;

            // --- F키 ---
            case S::F1: return K::F1;
            case S::F2: return K::F2;
            case S::F3: return K::F3;
            case S::F4: return K::F4;
            case S::F5: return K::F5;
            case S::F6: return K::F6;
            case S::F7: return K::F7;
            case S::F8: return K::F8;
            case S::F9: return K::F9;
            case S::F10: return K::F10;
            case S::F11: return K::F11;
            case S::F12: return K::F12;
            case S::F13: return K::F13;
            case S::F14: return K::F14;
            case S::F15: return K::F15;

            // --- 기타 ---
            case S::Pause: return K::Pause;

            default:
                return K::Unknown;
        }
    }

    core::model::Scan SfmlWindow::toScan(sf::Keyboard::Scancode s) {
        using S = sf::Keyboard::Scancode;
        using K = core::model::Scan;

        switch (s) {
            // --- 알파벳 ---
            case S::A: return K::A;
            case S::B: return K::B;
            case S::C: return K::C;
            case S::D: return K::D;
            case S::E: return K::E;
            case S::F: return K::F;
            case S::G: return K::G;
            case S::H: return K::H;
            case S::I: return K::I;
            case S::J: return K::J;
            case S::K: return K::K;
            case S::L: return K::L;
            case S::M: return K::M;
            case S::N: return K::N;
            case S::O: return K::O;
            case S::P: return K::P;
            case S::Q: return K::Q;
            case S::R: return K::R;
            case S::S: return K::S;
            case S::T: return K::T;
            case S::U: return K::U;
            case S::V: return K::V;
            case S::W: return K::W;
            case S::X: return K::X;
            case S::Y: return K::Y;
            case S::Z: return K::Z;

            // --- 숫자 (상단) ---
            case S::Num1: return K::Num1;
            case S::Num2: return K::Num2;
            case S::Num3: return K::Num3;
            case S::Num4: return K::Num4;
            case S::Num5: return K::Num5;
            case S::Num6: return K::Num6;
            case S::Num7: return K::Num7;
            case S::Num8: return K::Num8;
            case S::Num9: return K::Num9;
            case S::Num0: return K::Num0;

            // --- 기본 ---
            case S::Enter: return K::Enter;
            case S::Escape: return K::Escape;
            case S::Backspace: return K::Backspace;
            case S::Tab: return K::Tab;
            case S::Space: return K::Space;

            // --- 기호 ---
            case S::Hyphen: return K::Hyphen;
            case S::Equal: return K::Equal;
            case S::LBracket: return K::LBracket;
            case S::RBracket: return K::RBracket;
            case S::Backslash: return K::Backslash;
            case S::Semicolon: return K::Semicolon;
            case S::Apostrophe: return K::Apostrophe;
            case S::Grave: return K::Grave;
            case S::Comma: return K::Comma;
            case S::Period: return K::Period;
            case S::Slash: return K::Slash;

            // --- 기능키 ---
            case S::F1: return K::F1;
            case S::F2: return K::F2;
            case S::F3: return K::F3;
            case S::F4: return K::F4;
            case S::F5: return K::F5;
            case S::F6: return K::F6;
            case S::F7: return K::F7;
            case S::F8: return K::F8;
            case S::F9: return K::F9;
            case S::F10: return K::F10;
            case S::F11: return K::F11;
            case S::F12: return K::F12;
            case S::F13: return K::F13;
            case S::F14: return K::F14;
            case S::F15: return K::F15;
            case S::F16: return K::F16;
            case S::F17: return K::F17;
            case S::F18: return K::F18;
            case S::F19: return K::F19;
            case S::F20: return K::F20;
            case S::F21: return K::F21;
            case S::F22: return K::F22;
            case S::F23: return K::F23;
            case S::F24: return K::F24;

            // --- 상태 ---
            case S::CapsLock: return K::CapsLock;
            case S::PrintScreen: return K::PrintScreen;
            case S::ScrollLock: return K::ScrollLock;
            case S::Pause: return K::Pause;

            // --- 이동 ---
            case S::Insert: return K::Insert;
            case S::Home: return K::Home;
            case S::PageUp: return K::PageUp;
            case S::Delete: return K::Delete;
            case S::End: return K::End;
            case S::PageDown: return K::PageDown;

            // --- 방향 ---
            case S::Right: return K::Right;
            case S::Left: return K::Left;
            case S::Down: return K::Down;
            case S::Up: return K::Up;

            // --- 넘패드 ---
            case S::NumLock: return K::NumLock;
            case S::NumpadDivide: return K::NumpadDivide;
            case S::NumpadMultiply: return K::NumpadMultiply;
            case S::NumpadMinus: return K::NumpadMinus;
            case S::NumpadPlus: return K::NumpadPlus;
            case S::NumpadEqual: return K::NumpadEqual;
            case S::NumpadEnter: return K::NumpadEnter;
            case S::NumpadDecimal: return K::NumpadDecimal;
            case S::Numpad1: return K::Numpad1;
            case S::Numpad2: return K::Numpad2;
            case S::Numpad3: return K::Numpad3;
            case S::Numpad4: return K::Numpad4;
            case S::Numpad5: return K::Numpad5;
            case S::Numpad6: return K::Numpad6;
            case S::Numpad7: return K::Numpad7;
            case S::Numpad8: return K::Numpad8;
            case S::Numpad9: return K::Numpad9;
            case S::Numpad0: return K::Numpad0;

            // --- 특수 ---
            case S::NonUsBackslash: return K::NonUsBackslash;
            case S::Application: return K::Application;
            case S::Execute: return K::Execute;
            case S::ModeChange: return K::ModeChange;
            case S::Help: return K::Help;
            case S::Menu: return K::Menu;
            case S::Select: return K::Select;
            case S::Redo: return K::Redo;
            case S::Undo: return K::Undo;
            case S::Cut: return K::Cut;
            case S::Copy: return K::Copy;
            case S::Paste: return K::Paste;

            // --- 미디어 ---
            case S::VolumeMute: return K::VolumeMute;
            case S::VolumeUp: return K::VolumeUp;
            case S::VolumeDown: return K::VolumeDown;
            case S::MediaPlayPause: return K::MediaPlayPause;
            case S::MediaStop: return K::MediaStop;
            case S::MediaNextTrack: return K::MediaNextTrack;
            case S::MediaPreviousTrack: return K::MediaPreviousTrack;

            // --- Modifier ---
            case S::LControl: return K::LControl;
            case S::LShift: return K::LShift;
            case S::LAlt: return K::LAlt;
            case S::LSystem: return K::LSystem;
            case S::RControl: return K::RControl;
            case S::RShift: return K::RShift;
            case S::RAlt: return K::RAlt;
            case S::RSystem: return K::RSystem;

            // --- 브라우저 ---
            case S::Back: return K::Back;
            case S::Forward: return K::Forward;
            case S::Refresh: return K::Refresh;
            case S::Stop: return K::Stop;
            case S::Search: return K::Search;
            case S::Favorites: return K::Favorites;
            case S::HomePage: return K::HomePage;

            // --- 실행 ---
            case S::LaunchApplication1: return K::LaunchApplication1;
            case S::LaunchApplication2: return K::LaunchApplication2;
            case S::LaunchMail: return K::LaunchMail;
            case S::LaunchMediaSelect: return K::LaunchMediaSelect;

            default:
                return K::Unknown;
        }
    }
}
