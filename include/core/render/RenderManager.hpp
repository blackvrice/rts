//
// Created by black on 25. 12. 27..
//

// render/RenderManager.hpp
#pragma once
#include "IRenderManager.hpp"

namespace rts::render {

    class RenderManager : public IRenderManager {
    public:
        void execute(const RenderQueue& queue) override;

    private:
        void draw(const DrawRect& r);
        void draw(const DrawText& t);
        void draw(const DrawSprite& s);
    };

} // namespace rts::render
