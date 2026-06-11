#pragma once

#include <array>
#include <memory>
#include <string>
#include <unordered_map>

#include "core/model/PlayerResourceState.hpp"

struct ImGuiContext;
struct ImVec2;

namespace sf {
    class RenderWindow;
    class Texture;
}

namespace rts::core::command {
    class UICommandBus;
}

namespace rts::core::render {
    struct UpdateHudSelection;
    struct UpdateMinimap;
}

namespace rts::platform::sfml {
    class SfmlHudOverlay {
    public:
        explicit SfmlHudOverlay(core::command::UICommandBus& uiBus);
        ~SfmlHudOverlay();

        SfmlHudOverlay(const SfmlHudOverlay&) = delete;
        SfmlHudOverlay& operator=(const SfmlHudOverlay&) = delete;

        void render(
            sf::RenderWindow& window,
            const core::model::PlayerResourceState& resources,
            const core::render::UpdateHudSelection& selection,
            const core::render::UpdateMinimap& minimap);

    private:
        void initialize(sf::RenderWindow& window);
        void shutdown();
        void syncMouseButtons();
        const sf::Texture* texture(const std::string& relativePath);

        static void applyStyle();
        void drawHud(
            const ImVec2& displaySize,
            const core::model::PlayerResourceState& resources,
            const core::render::UpdateHudSelection& selection,
            const core::render::UpdateMinimap& minimap);

        ImGuiContext* m_context = nullptr;
        std::array<bool, 3> m_mouseButtons{};
        std::unordered_map<std::string, std::unique_ptr<sf::Texture>> m_textures;
        core::command::UICommandBus& m_uiBus;
        std::string m_lastCommand = "Move";
    };
}
