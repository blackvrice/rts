//
// Created by black on 25. 12. 27..
//

#include <core/render/IRenderManager.hpp>
#include <SFML/Graphics/RenderWindow.hpp>

#include "SfmlWindow.hpp"
#include "core/render/RenderContext.hpp"

namespace rts::platform::sfml {
    class SfmlRenderManager : public core::render::IRenderManager {
    public:
        void execute(const core::render::RenderQueue& queue, const core::render::RenderContext& ctx) override;

    private:
        static void draw(sf::RenderWindow& window, const core::render::DrawRect& r);

        static void draw(sf::RenderWindow& window, const core::render::DrawText& t);

        static void draw(sf::RenderWindow& window, const core::render::DrawSprite& s);


    };
}
