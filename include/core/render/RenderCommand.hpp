//
// Created by black on 25. 12. 27..
//
// render/RenderCommand.hpp
#pragma once
#include <cstdint>
#include <string>
#include <variant>

#include "core/font/FontTypes.hpp"
#include "core/model/Rect.hpp"

namespace rts::core::render {

    enum class RenderLayer : uint8_t {
        World = 0,
        UI    = 1
    };

    struct DrawRect {
        model::Rect rect;
        uint32_t border_color;
        uint32_t color;
    };

    struct DrawText {
        model::Vector2D pos;
        uint32_t color;
        font::FontId fontId;
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
