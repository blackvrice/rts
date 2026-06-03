//
// Created by black on 25. 12. 27..
//

#include <core/render/IRenderManager.hpp>
#include <SFML/Graphics/RenderWindow.hpp>

namespace rts::core::render {
    struct DrawCircle;
    struct DrawRect;
    struct DrawSprite;
    struct DrawText;
}

namespace rts::platform::sfml {
    class SfmlRenderManager : public core::render::IRenderManager {
    public:
        void execute(const core::render::RenderQueue& queue, const core::render::RenderContext& ctx) override;

    private:
        static void draw(sf::RenderWindow& window, const core::render::DrawRect& r);

        static void draw(sf::RenderWindow& window, const core::render::DrawText& t);

        static void draw(sf::RenderWindow& window, const core::render::DrawSprite& s);

        static void draw(sf::RenderWindow& window, const core::render::DrawCircle& c);

    };
}
