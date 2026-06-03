#pragma once

#include <array>
#include <memory>
#include <string>
#include <unordered_map>

struct ImGuiContext;
struct ImVec2;

namespace sf {
    class RenderWindow;
    class Texture;
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
        const sf::Texture* texture(const std::string& relativePath);

        static void applyStyle();
        void drawHud(const ImVec2& displaySize);

        ImGuiContext* m_context = nullptr;
        std::array<bool, 3> m_mouseButtons{};
        std::unordered_map<std::string, std::unique_ptr<sf::Texture>> m_textures;
        std::string m_lastCommand = "Move";
    };
}
