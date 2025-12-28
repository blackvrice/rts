//
// Created by black on 25. 12. 27..
//
// render/RenderCommand.hpp
#pragma once
#include <variant>

namespace rts::core::render {

    enum class RenderLayer : uint8_t {
        World = 0,
        UI    = 1
    };

    struct DrawRect {
        float x, y, w, h;
        uint32_t color;
    };

    struct DrawText {
        float x;
        float y;
        uint32_t color;     // ARGB
        int fontId;
        int size;
        std::string text;
    };


    struct DrawSprite {
        float x;
        float y;
        float w;
        float h;
        int textureId;
    };

    using RenderCommandData = std::variant<
        DrawRect,
        DrawText,
        DrawSprite
    >;

    struct RenderCommand {
        RenderLayer layer;
        int zOrder;
        RenderCommandData data;
    };

} // namespace rts::render
