//
// Created by black on 25. 12. 27..
//
#pragma once
#include "RenderContext.hpp"
#include "RenderQueue.hpp"

namespace rts::core::render {

    class IRenderManager {
    public:
        virtual ~IRenderManager() = default;
        virtual void execute(
            const RenderQueue& queue,
            const RenderContext& ctx
        ) = 0;
    };

} // namespace rts::render