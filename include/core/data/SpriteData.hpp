#pragma once

#include <string>

namespace rts::core::data {
    // Design-time description of a drawable sprite/animation clip. Loaded from
    // data/animations.json and consumed by the view models when they emit
    // DrawSprite commands, so visuals can be added/removed/changed without code.
    struct SpriteClip {
        std::string texture;        // path relative to the asset (Tiny Swords) root
        int frameCount { 1 };       // number of horizontal frames in the sheet
        float fps { 0.0f };         // playback speed; 0 = static (first frame)
        int sourceX { 0 };
        int sourceY { 0 };
        int sourceW { 0 };          // 0 = use the whole texture as one frame
        int sourceH { 0 };
        float displayW { 96.0f };   // world-space draw size
        float displayH { 96.0f };
        // Anchor offset subtracted from the entity position to get the draw
        // origin (drawX = pos.x - anchorX, drawY = pos.y - anchorY).
        float anchorX { 48.0f };
        float anchorY { 96.0f };
        bool trim { false };        // trim transparent border, bottom-centered
    };
}
