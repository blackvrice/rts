#pragma once

#include <array>
#include <string>

struct ImGuiContext;
struct ImVec2;

namespace sf {
    class RenderWindow;
}

namespace rts::platform::sfml {
    class SfmlHudOverlay {
    public:
        SfmlHudOverlay() = default;
        ~SfmlHudOverlay();

        SfmlHudOverlay(const SfmlHudOverlay&) = delete;
        SfmlHudOverlay& operator=(const SfmlHudOverlay&) = delete;

        void render(sf::RenderWindow& window);

    private:
        void initialize(sf::RenderWindow& window);
        void shutdown();
        void syncMouseButtons();

        static void applyStyle();
        void drawHud(const ImVec2& displaySize);

        ImGuiContext* m_context = nullptr;
        std::array<bool, 3> m_mouseButtons{};
        std::string m_lastCommand = "Move";
    };
}
