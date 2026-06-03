//
// Created by black on 25. 12. 27..
//

#pragma once

#include <memory>

#include <core/render/IRenderManager.hpp>
#include <SFML/Graphics/RenderWindow.hpp>

namespace rts::core::render {
    struct DrawCircle;
    struct DrawRect;
    struct DrawSprite;
    struct DrawText;
}

namespace rts::platform::sfml {
    class SfmlHudOverlay;

    class SfmlRenderManager : public core::render::IRenderManager {
    public:
        SfmlRenderManager();
        ~SfmlRenderManager() override;

        void execute(const core::render::RenderQueue& queue, const core::render::RenderContext& ctx) override;

    private:
        static void draw(sf::RenderWindow& window, const core::render::DrawRect& r);

        static void draw(sf::RenderWindow& window, const core::render::DrawText& t);

        static void draw(sf::RenderWindow& window, const core::render::DrawSprite& s);

        static void draw(sf::RenderWindow& window, const core::render::DrawCircle& c);

        std::unique_ptr<SfmlHudOverlay> m_hud;
    };
}
