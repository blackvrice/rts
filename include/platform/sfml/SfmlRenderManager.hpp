//
// Created by black on 25. 12. 27..
//

#pragma once

#include <memory>

#include <core/render/IRenderManager.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/RenderTexture.hpp>

namespace rts::core::command {
    class UICommandBus;
    class AudioCommandBus;
}

namespace rts::core::render {
    struct DrawCircle;
    struct DrawRect;
    struct DrawSprite;
    struct DrawText;
    struct DrawFog;
    struct UpdateHudResources;
    struct UpdateHudSelection;
    struct UpdateHudCursor;
    struct UpdateMinimap;
    struct PlaySound;
}

namespace rts::platform::sfml {
    class SfmlHudOverlay;

    class SfmlRenderManager : public core::render::IRenderManager {
    public:
        SfmlRenderManager(core::command::UICommandBus& uiBus,
                          core::command::AudioCommandBus& audioBus);
        ~SfmlRenderManager() override;

        void execute(const core::render::RenderQueue& queue, const core::render::RenderContext& ctx) override;

    private:
        static void draw(sf::RenderWindow& window, const core::render::DrawRect& r);

        static void draw(sf::RenderWindow& window, const core::render::DrawText& t);

        static void draw(sf::RenderWindow& window, const core::render::DrawSprite& s);

        static void draw(sf::RenderWindow& window, const core::render::DrawCircle& c);

        // Non-static: composites the shroud into m_fogTexture so the visible area
        // can be revealed as smooth circles before being drawn over the world.
        void draw(sf::RenderWindow& window, const core::render::DrawFog& fog);

        static void draw(sf::RenderWindow& window, const core::render::UpdateHudResources& resources);

        static void draw(sf::RenderWindow& window, const core::render::UpdateHudSelection& selection);

        static void draw(sf::RenderWindow& window, const core::render::UpdateHudCursor& cursor);

        // Non-static: forwards the cue to the audio thread via m_audioBus.
        void draw(sf::RenderWindow& window, const core::render::PlaySound& sound);

        static void draw(sf::RenderWindow& window, const core::render::UpdateMinimap& minimap);

        std::unique_ptr<SfmlHudOverlay> m_hud;
        core::command::AudioCommandBus& m_audioBus;
        // Offscreen mask for smooth fog: shroud is rendered here, vision circles are
        // punched out, then the result is drawn full-screen over the world.
        std::unique_ptr<sf::RenderTexture> m_fogTexture;
    };
}
