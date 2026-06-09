//
// Created by black on 25. 12. 27..
//
// render/RenderCommand.hpp
#pragma once
#include <cstdint>
#include <string>
#include <variant>

#include "core/font/FontTypes.hpp"
#include "core/model/PlayerResourceState.hpp"
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
        // Texture file path relative to the asset root; the renderer loads and
        // caches by path. Empty means nothing is drawn.
        std::string texturePath {};
        int sourceX { 0 };
        int sourceY { 0 };
        int sourceW { 0 };
        int sourceH { 0 };
        int frameCount { 1 };
        float framesPerSecond { 0.0f };
        bool trimTransparent { false };
        bool showInHud { false };
        float rotation { 0.0f };
    };

    struct DrawCircle {
        float cx;      // center x
        float cy;      // center y
        float radius;
        uint32_t border_color;
        uint32_t color;
    };

    struct UpdateHudResources {
        model::PlayerResourceState resources;
    };

    // Kind of the primary selected element, so the HUD can show context-relevant
    // command buttons (a barracks gets Train, a worker gets Build/Gather, etc.).
    enum class HudSelectionKind : uint8_t {
        None,
        Worker,
        CombatUnit,
        Building,
        Resource
    };

    struct UpdateHudSelection {
        int selectedCount {};
        bool hasPrimaryUnit {};
        std::string primaryName { "No unit selected" };
        std::string action { "None" };
        float hp {};
        float maxHp {};
        bool hasCombatStats {};
        float attackDamage {};
        float armor {};
        float attackRange {};
        model::Vector2D position {};
        HudSelectionKind kind { HudSelectionKind::None };
        bool canProduce { false };  // building whose produces list is non-empty
    };

    struct UpdateHudCursor {
        // Semantic cursor state; the renderer resolves "cursor.<key>" against the
        // sprite registry (data/animations.json) to a texture.
        std::string cursorKey { "default" };
    };

    using RenderCommandData = std::variant<
        DrawRect,
        DrawText,
        DrawSprite,
        DrawCircle,
        UpdateHudResources,
        UpdateHudSelection,
        UpdateHudCursor
    >;

    struct RenderCommand {
        RenderLayer layer;
        int zOrder;
        RenderCommandData data;
    };


} // namespace rts::render
