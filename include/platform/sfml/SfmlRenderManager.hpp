//
// Created by black on 25. 12. 27..
//

#include <core/render/IRenderManager.hpp>
#include <SFML/Graphics/RenderWindow.hpp>

#include "core/render/RenderContext.hpp"

namespace rts::platform::sfml {
    class SfmlRenderManager : public core::render::IRenderManager {
    public:
        void execute(const core::render::RenderQueue& queue, const core::render::RenderContext& ctx) override;

    private:
        void draw(sf::RenderWindow*, const core::render::DrawRect& r);
        void draw(sf::RenderWindow*, const core::render::DrawText& t);
        void draw(sf::RenderWindow*, const core::render::DrawSprite& s);


    };
}
