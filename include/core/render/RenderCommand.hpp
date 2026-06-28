//
// Created by black on 25. 12. 27..
//
// render/RenderCommand.hpp
#pragma once
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

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
        bool flipX { false };  // mirror horizontally (unit facing left)
    };

    struct DrawCircle {
        float cx;      // center x
        float cy;      // center y
        float radius;
        uint32_t border_color;
        uint32_t color;
    };

    // One currently-seeing source (unit/building) for the smooth fog reveal, in
    // world coordinates. The renderer punches a soft circle of this radius out of
    // the shroud so the visible boundary is pixel-smooth rather than tile-blocky.
    struct FogVisionSource {
        float cx;
        float cy;
        float radius;  // world units
    };

    // Fog-of-war shroud for the world layer. The explored/unexplored memory stays
    // tile-resolution (states grid, mirrors FogOfWar::State, row-major fogW*fogH),
    // but the currently-visible area is rendered from `sources` as smooth circles
    // composited on the GPU. Emitted once per frame above the world sprites.
    struct DrawFog {
        float tileSize { 0.0f };
        int fogW { 0 };
        int fogH { 0 };
        std::vector<uint8_t> states {};
        std::vector<FogVisionSource> sources {};
        // 0xRRGGBBAA shroud tints; alpha is the last byte.
        uint32_t exploredColor { 0x05070A66u };    // seen before, not in sight now
        uint32_t unexploredColor { 0x05070AE6u };  // never seen
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

    // One entry per selected unit, for the multi-selection portrait strip.
    struct HudPortrait {
        HudSelectionKind kind { HudSelectionKind::None };
        float hp01 { 1.0f };  // current/max health, 0..1 (drives the portrait HP tint/bar)
        int unitTypeId { -1 };      // unit avatar (units only; -1 otherwise)
        int buildingTypeId { -1 };  // building art (buildings only; -1 otherwise)
        int team { 0 };             // owning team (selects the building art's colour)
        std::string iconPath {};    // data-driven portrait path (from JSON); preferred when set
        // EntityId of the represented element, so clicking the portrait re-selects it.
        std::uint32_t entityIndex { 0 };
        std::uint32_t entityGeneration { 0 };
    };

    // One constructible building shown in a worker's build submenu.
    struct HudBuildOption {
        int buildingTypeId { -1 };
        std::string label;
        bool locked { false };  // requirements unmet or unaffordable
        std::string iconKey {};  // data/animations.json sprite key for the command HUD
    };

    // One trainable unit shown in a production building's command card.
    struct HudTrainOption {
        int unitTypeId { -1 };
        std::string label;
        bool locked { false };  // requirements unmet or unaffordable
        std::string iconKey {};  // data/animations.json sprite key for the command HUD
    };

    // One unit currently queued inside a selected production building.
    struct HudProductionQueueItem {
        int unitTypeId { -1 };
        std::string label;
        std::string iconKey {};
        float progress01 { 0.0f };  // front item uses train progress; queued items stay 0
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
        bool canProduce { false };   // completed building whose produces list is non-empty
        bool producesUnits { false };  // building type can train units (even if not yet ready)
        bool trainAffordable { true };  // player can pay for the building's default unit
        // Building progress overlays (0..1; <0 means not applicable).
        bool underConstruction { false };
        float buildProgress01 { 0.0f };   // construction completion
        float trainProgress01 { 0.0f };   // front-of-queue training completion
        int trainQueueCount { 0 };
        // Portraits for every selected unit (capped); portraits.size()<=selectedCount.
        std::vector<HudPortrait> portraits {};
        // Constructible buildings for a worker's build submenu.
        std::vector<HudBuildOption> buildOptions {};
        // Trainable units for a selected production building.
        std::vector<HudTrainOption> trainOptions {};
        // Actual in-progress/queued units for a selected production building.
        std::vector<HudProductionQueueItem> trainQueue {};
    };

    struct UpdateHudCursor {
        // Semantic cursor state; the renderer resolves "cursor.<key>" against the
        // sprite registry (data/animations.json) to a texture.
        std::string cursorKey { "default" };
    };

    struct PlaySound {
        // Semantic cue resolved by the platform renderer. The SFML backend currently
        // uses synthesized one-shot tones, so no external audio assets are required.
        std::string cue {};
        float volume { 70.0f };
    };

    // A single blip on the minimap; position is normalized 0..1 over the world.
    struct MinimapDot {
        float u { 0.0f };
        float v { 0.0f };
        uint8_t team { 0 };  // 0 = player, 1 = enemy, 2 = neutral/resource
    };

    // Snapshot the HUD needs to draw the minimap and translate clicks back to world
    // coordinates. Fog states (size fogW*fogH, row-major) mirror FogOfWar::State.
    struct UpdateMinimap {
        float worldW { 0.0f };
        float worldH { 0.0f };
        // Camera viewport as a normalized rect over the world (for the viewport box).
        float camU { 0.0f };
        float camV { 0.0f };
        float camW { 0.0f };
        float camH { 0.0f };
        int fogW { 0 };
        int fogH { 0 };
        std::vector<uint8_t> fog {};
        std::vector<MinimapDot> dots {};
    };

    using RenderCommandData = std::variant<
        DrawRect,
        DrawText,
        DrawSprite,
        DrawCircle,
        DrawFog,
        UpdateHudResources,
        UpdateHudSelection,
        UpdateHudCursor,
        PlaySound,
        UpdateMinimap
    >;

    struct RenderCommand {
        RenderLayer layer;
        int zOrder;
        RenderCommandData data;
    };


} // namespace rts::render
