#include "platform/sfml/SfmlHudOverlay.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Texture.hpp>

#include "core/command/UICommand.hpp"
#include "core/command/UICommandBus.hpp"
#include "core/data/DataRegistry.hpp"
#include "core/render/RenderCommand.hpp"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_win32.h"
#include "platform/sfml/SfmlAssetPaths.hpp"

namespace {
    constexpr ImU32 kConsoleFill = IM_COL32(10, 18, 26, 235);
    constexpr ImU32 kPanelFill = IM_COL32(18, 34, 48, 230);
    constexpr ImU32 kPanelDark = IM_COL32(5, 10, 15, 245);
    constexpr ImU32 kPanelHigh = IM_COL32(82, 145, 158, 230);
    constexpr ImU32 kPanelEdge = IM_COL32(145, 205, 212, 210);
    constexpr ImU32 kTextMain = IM_COL32(218, 240, 232, 255);
    constexpr ImU32 kTextDim = IM_COL32(126, 165, 162, 255);
    constexpr ImU32 kMineral = IM_COL32(75, 196, 235, 255);
    constexpr ImU32 kGas = IM_COL32(78, 218, 148, 255);
    constexpr ImU32 kWarning = IM_COL32(236, 198, 81, 255);
    constexpr ImU32 kDanger = IM_COL32(228, 86, 74, 255);

    // HUD ADJUST: swap these paths when testing different Tiny Swords HUD png pieces.
    constexpr const char* kWoodTable = "UI Elements/UI Elements/Wood Table/WoodTable.png";
    constexpr const char* kWoodSlots = "UI Elements/UI Elements/Wood Table/WoodTable_Slots.png";
    constexpr const char* kBanner = "UI Elements/UI Elements/Banners/Banner.png";
    constexpr const char* kBannerSlots = "UI Elements/UI Elements/Banners/Banner_Slots.png";
    constexpr const char* kButtonRegular = "UI Elements/UI Elements/Buttons/SmallBlueSquareButton_Regular.png";
    constexpr const char* kButtonPressed = "UI Elements/UI Elements/Buttons/SmallBlueSquareButton_Pressed.png";
    constexpr const char* kBarBase = "UI Elements/UI Elements/Bars/BigBar_Base.png";
    constexpr const char* kBarFill = "UI Elements/UI Elements/Bars/BigBar_Fill.png";
    constexpr const char* kAvatar = "UI Elements/UI Elements/Human Avatars/Avatars_01.png";
    constexpr const char* kSwords = "UI Elements/UI Elements/Swords/Swords.png";

    struct SourceRect {
        float x;
        float y;
        float w;
        float h;
    };

    struct HudCommandButton {
        // What a click does: fire a gameplay action, open/close the worker build
        // submenu, arm a specific building, or train a specific unit.
        enum class Kind { Action, OpenBuildMenu, SelectBuild, CloseBuildMenu, SelectTrain };
        std::string label;
        rts::core::command::GameplayInputAction action {};
        const char* hotkey { "" };  // shown in the button corner; matches starCraftHotkeyAction
        bool locked { false };  // greyed out and non-interactive (prereq not met)
        Kind kind { Kind::Action };
        int payloadId { -1 };  // buildingTypeId (SelectBuild) or unitTypeId (SelectTrain)
        std::string iconKey;  // data/animations.json key, e.g. command.move
    };

    HudCommandButton commandButton(
        std::string label,
        const rts::core::command::GameplayInputAction action,
        const char* hotkey,
        std::string iconKey,
        const HudCommandButton::Kind kind = HudCommandButton::Kind::Action
    ) {
        HudCommandButton button;
        button.label = std::move(label);
        button.action = action;
        button.hotkey = hotkey;
        button.kind = kind;
        button.iconKey = std::move(iconKey);
        return button;
    }

    struct TrimCacheKey {
        const sf::Texture* texture;
        int x;
        int y;
        int w;
        int h;

        bool operator==(const TrimCacheKey& other) const {
            return texture == other.texture &&
                   x == other.x &&
                   y == other.y &&
                   w == other.w &&
                   h == other.h;
        }
    };

    struct TrimCacheKeyHash {
        std::size_t operator()(const TrimCacheKey& key) const {
            std::size_t result = std::hash<const sf::Texture*>{}(key.texture);
            const auto combine = [&result](const std::size_t value) {
                result ^= value + 0x9e3779b97f4a7c15ULL + (result << 6) + (result >> 2);
            };
            combine(std::hash<int>{}(key.x));
            combine(std::hash<int>{}(key.y));
            combine(std::hash<int>{}(key.w));
            combine(std::hash<int>{}(key.h));
            return result;
        }
    };

    constexpr SourceRect kBlueSwordIcon{0.0f, 0.0f, 96.0f, 128.0f};

    std::unordered_map<const sf::Texture*, sf::Image> g_trimImages;
    std::unordered_map<TrimCacheKey, SourceRect, TrimCacheKeyHash> g_trimCache;

    std::filesystem::path tinySwordsRoot() {
        return std::filesystem::path(rts::platform::sfml::TinySwordsRoot);
    }

    std::string formatNumber(const int value) {
        std::string text = std::to_string(value);
        for (int insertAt = static_cast<int>(text.size()) - 3; insertAt > 0; insertAt -= 3) {
            text.insert(static_cast<std::size_t>(insertAt), ",");
        }
        return text;
    }

    std::string formatFood(const rts::core::model::PlayerResourceState& resources) {
        return std::to_string(resources.foodUsed) + "/" + std::to_string(resources.foodCapacity);
    }

    std::string formatRounded(const float value) {
        return std::to_string(static_cast<int>(std::lround(value)));
    }

    std::string formatPosition(const rts::core::model::Vector2D& position) {
        return formatRounded(position.x) + ", " + formatRounded(position.y);
    }

    ImVec2 operator+(const ImVec2& a, const ImVec2& b) {
        return {a.x + b.x, a.y + b.y};
    }

    ImTextureRef textureRef(const sf::Texture& texture) {
        return ImTextureRef(static_cast<ImTextureID>(texture.getNativeHandle()));
    }

    ImVec2 uv(const sf::Texture& texture, const float x, const float y) {
        const auto size = texture.getSize();
        return {
            x / static_cast<float>(size.x),
            y / static_cast<float>(size.y)
        };
    }

    void drawImage(ImDrawList& drawList, const sf::Texture* texture, const ImVec2 min, const ImVec2 max, const ImU32 tint = IM_COL32_WHITE) {
        if (!texture) {
            return;
        }

        drawList.AddImage(textureRef(*texture), min, max, {0.0f, 0.0f}, {1.0f, 1.0f}, tint);
    }

    void drawImageRect(ImDrawList& drawList, const sf::Texture* texture, const SourceRect src, const ImVec2 min, const ImVec2 max, const ImU32 tint = IM_COL32_WHITE) {
        if (!texture) {
            return;
        }

        drawList.AddImage(
            textureRef(*texture),
            min,
            max,
            uv(*texture, src.x, src.y),
            uv(*texture, src.x + src.w, src.y + src.h),
            tint
        );
    }

    const sf::Image& trimImageFor(const sf::Texture& texture) {
        if (const auto it = g_trimImages.find(&texture); it != g_trimImages.end()) {
            return it->second;
        }

        auto [it, _] = g_trimImages.emplace(&texture, texture.copyToImage());
        return it->second;
    }

    SourceRect trimTransparentSourceRect(const sf::Texture& texture, const SourceRect src) {
        const auto textureSize = texture.getSize();
        const int x0 = std::clamp(static_cast<int>(std::round(src.x)), 0, static_cast<int>(textureSize.x));
        const int y0 = std::clamp(static_cast<int>(std::round(src.y)), 0, static_cast<int>(textureSize.y));
        const int x1 = std::clamp(static_cast<int>(std::round(src.x + src.w)), x0, static_cast<int>(textureSize.x));
        const int y1 = std::clamp(static_cast<int>(std::round(src.y + src.h)), y0, static_cast<int>(textureSize.y));

        const TrimCacheKey key{x0 < x1 && y0 < y1 ? &texture : nullptr, x0, y0, x1 - x0, y1 - y0};
        if (const auto it = g_trimCache.find(key); it != g_trimCache.end()) {
            return it->second;
        }

        const sf::Image& image = trimImageFor(texture);
        const auto imageSize = image.getSize();
        const std::uint8_t* pixels = image.getPixelsPtr();
        int minX = x1;
        int minY = y1;
        int maxX = x0 - 1;
        int maxY = y0 - 1;

        for (int y = y0; y < y1; ++y) {
            for (int x = x0; x < x1; ++x) {
                const std::size_t alphaIndex = (static_cast<std::size_t>(y) * imageSize.x + static_cast<std::size_t>(x)) * 4U + 3U;
                if (pixels[alphaIndex] == 0U) {
                    continue;
                }

                minX = std::min(minX, x);
                minY = std::min(minY, y);
                maxX = std::max(maxX, x);
                maxY = std::max(maxY, y);
            }
        }

        // HUD ADJUST: transparent source padding is trimmed here before 9-slice pieces are drawn.
        const SourceRect trimmed = maxX < minX || maxY < minY
            ? SourceRect{src.x, src.y, src.w, src.h}
            : SourceRect{
                static_cast<float>(minX),
                static_cast<float>(minY),
                static_cast<float>(maxX - minX + 1),
                static_cast<float>(maxY - minY + 1)
            };
        g_trimCache.emplace(key, trimmed);
        return trimmed;
    }

    SourceRect spriteClipSourceRect(
        const sf::Texture& texture,
        const rts::core::data::SpriteClip& clip
    ) {
        const auto textureSize = texture.getSize();
        SourceRect src{
            static_cast<float>(clip.sourceX),
            static_cast<float>(clip.sourceY),
            clip.sourceW > 0 ? static_cast<float>(clip.sourceW) : static_cast<float>(textureSize.x),
            clip.sourceH > 0 ? static_cast<float>(clip.sourceH) : static_cast<float>(textureSize.y)
        };
        if (clip.trim) {
            src = trimTransparentSourceRect(texture, src);
        }
        return src;
    }

    void drawSpriteClip(
        ImDrawList& drawList,
        const sf::Texture* texture,
        const rts::core::data::SpriteClip& clip,
        const ImVec2 min,
        const ImVec2 max,
        const ImU32 tint = IM_COL32_WHITE
    ) {
        if (!texture) {
            return;
        }
        drawImageRect(drawList, texture, spriteClipSourceRect(*texture, clip), min, max, tint);
    }

    void drawTiledImageRect(
        ImDrawList& drawList,
        const sf::Texture* texture,
        const SourceRect src,
        const ImVec2 min,
        const ImVec2 max,
        const ImVec2 tileSize,
        const ImU32 tint = IM_COL32_WHITE
    ) {
        if (!texture || tileSize.x <= 0.0f || tileSize.y <= 0.0f || max.x <= min.x || max.y <= min.y) {
            return;
        }

        for (float y = min.y; y < max.y;) {
            const float h = std::min(tileSize.y, max.y - y);
            const float v = h / tileSize.y;

            for (float x = min.x; x < max.x;) {
                const float w = std::min(tileSize.x, max.x - x);
                const float u = w / tileSize.x;

                // Crop the source UV on the final tile so repeated HUD pieces end flush.
                drawImageRect(
                    drawList,
                    texture,
                    SourceRect{src.x, src.y, src.w * u, src.h * v},
                    {x, y},
                    {x + w, y + h},
                    tint
                );

                x += w;
            }

            y += h;
        }
    }

    // HUD ADJUST: sourceEdge is the png corner size, targetEdge is the on-screen corner size.
    void drawNineSlice(ImDrawList& drawList, const sf::Texture* texture, const ImVec2 min, const ImVec2 max, const float sourceEdge, const float targetEdge, const ImU32 tint = IM_COL32_WHITE, const bool trimTransparentSource = true) {
        if (!texture || sourceEdge <= 0.0f) {
            return;
        }

        const auto textureSize = texture->getSize();
        const float sourceW = static_cast<float>(textureSize.x);
        const float sourceH = static_cast<float>(textureSize.y);
        const float sourceCenterW = sourceW - sourceEdge * 2.0f;
        const float sourceCenterH = sourceH - sourceEdge * 2.0f;
        const float targetW = max.x - min.x;
        const float targetH = max.y - min.y;
        const float edge = std::min({targetEdge, targetW * 0.5f, targetH * 0.5f});
        const float scale = edge / sourceEdge;

        const std::array<float, 4> sx{0.0f, sourceEdge, sourceEdge + sourceCenterW, sourceW};
        const std::array<float, 4> sy{0.0f, sourceEdge, sourceEdge + sourceCenterH, sourceH};
        const std::array<float, 4> dx{min.x, min.x + edge, max.x - edge, max.x};
        const std::array<float, 4> dy{min.y, min.y + edge, max.y - edge, max.y};

        const auto trimRect = [&](const SourceRect rect) {
            return trimTransparentSource ? trimTransparentSourceRect(*texture, rect) : rect;
        };

        drawImageRect(drawList, texture, trimRect(SourceRect{sx[0], sy[0], sourceEdge, sourceEdge}), {dx[0], dy[0]}, {dx[1], dy[1]}, tint);
        drawImageRect(drawList, texture, trimRect(SourceRect{sx[2], sy[0], sourceEdge, sourceEdge}), {dx[2], dy[0]}, {dx[3], dy[1]}, tint);
        drawImageRect(drawList, texture, trimRect(SourceRect{sx[0], sy[2], sourceEdge, sourceEdge}), {dx[0], dy[2]}, {dx[1], dy[3]}, tint);
        drawImageRect(drawList, texture, trimRect(SourceRect{sx[2], sy[2], sourceEdge, sourceEdge}), {dx[2], dy[2]}, {dx[3], dy[3]}, tint);

        if (sourceCenterW > 0.0f) {
            const SourceRect top = trimRect(SourceRect{sx[1], sy[0], sourceCenterW, sourceEdge});
            const SourceRect bottom = trimRect(SourceRect{sx[1], sy[2], sourceCenterW, sourceEdge});
            drawTiledImageRect(drawList, texture, top, {dx[1], dy[0]}, {dx[2], dy[1]}, {top.w * scale, edge}, tint);
            drawTiledImageRect(drawList, texture, bottom, {dx[1], dy[2]}, {dx[2], dy[3]}, {bottom.w * scale, edge}, tint);
        }

        if (sourceCenterH > 0.0f) {
            const SourceRect left = trimRect(SourceRect{sx[0], sy[1], sourceEdge, sourceCenterH});
            const SourceRect right = trimRect(SourceRect{sx[2], sy[1], sourceEdge, sourceCenterH});
            drawTiledImageRect(drawList, texture, left, {dx[0], dy[1]}, {dx[1], dy[2]}, {edge, left.h * scale}, tint);
            drawTiledImageRect(drawList, texture, right, {dx[2], dy[1]}, {dx[3], dy[2]}, {edge, right.h * scale}, tint);
        }

        if (sourceCenterW > 0.0f && sourceCenterH > 0.0f) {
            const SourceRect center = trimRect(SourceRect{sx[1], sy[1], sourceCenterW, sourceCenterH});
            drawTiledImageRect(
                drawList,
                texture,
                center,
                {dx[1], dy[1]},
                {dx[2], dy[2]},
                {center.w * scale, center.h * scale},
                tint
            );
        }
    }

    // HUD ADJUST: panel title position, border color, and dark overlay are controlled here.
    void drawPanelFrame(ImDrawList& drawList, const ImVec2 min, const ImVec2 max, const char* title, const sf::Texture* texture, const float sourceEdge, const float targetEdge) {
        if (texture) {
            drawNineSlice(drawList, texture, min, max, sourceEdge, targetEdge, IM_COL32(255, 255, 255, 238));
            drawList.AddRectFilled(min, max, IM_COL32(12, 11, 8, 72), 2.0f);
        } else {
            drawList.AddRectFilled(min, max, kPanelFill, 2.0f);
        }

        drawList.AddRect(min, max, kPanelEdge, 2.0f, 0, 2.0f);
        drawList.AddRect(min + ImVec2{4.0f, 4.0f}, max + ImVec2{-4.0f, -4.0f}, kPanelHigh, 1.0f, 0, 1.0f);

        const ImVec2 titlePos{min.x + 12.0f, min.y + 8.0f};
        drawList.AddText(titlePos, kTextDim, title);
        drawList.AddLine({min.x + 8.0f, min.y + 30.0f}, {max.x - 8.0f, min.y + 30.0f}, IM_COL32(70, 120, 132, 180), 1.0f);
    }

    void drawResourcePill(ImDrawList& drawList, const ImVec2 min, const ImVec2 size, const char* label, const char* value, const ImU32 color, const sf::Texture* icon, const SourceRect* iconSource = nullptr, const ImU32 valueColor = kTextMain) {
        const ImVec2 max{min.x + size.x, min.y + size.y};
        drawList.AddRectFilled(min, max, IM_COL32(8, 16, 22, 232), 2.0f);
        drawList.AddRect(min, max, IM_COL32(70, 126, 138, 210), 2.0f, 0, 1.0f);

        if (icon) {
            const ImVec2 iconMin{min.x + 8.0f, min.y + 5.0f};
            const ImVec2 iconMax{min.x + 30.0f, min.y + 27.0f};
            if (iconSource) {
                drawImageRect(drawList, icon, *iconSource, iconMin, iconMax);
            } else {
                drawImage(drawList, icon, iconMin, iconMax);
            }
        } else {
            drawList.AddCircleFilled({min.x + 18.0f, min.y + size.y * 0.5f}, 6.0f, color, 16);
        }

        drawList.AddText({min.x + 32.0f, min.y + 7.0f}, kTextDim, label);

        const ImVec2 valueSize = ImGui::CalcTextSize(value);
        drawList.AddText({max.x - valueSize.x - 12.0f, min.y + 7.0f}, valueColor, value);
    }

    // Inner drawable rect of the minimap panel (where the map proper is rendered).
    void minimapInnerRect(const ImVec2 min, const ImVec2 max, ImVec2& outMin, ImVec2& outMax) {
        outMin = ImVec2{min.x + 14.0f, min.y + 38.0f};
        outMax = ImVec2{max.x - 14.0f, max.y - 14.0f};
    }

    void drawMiniMap(ImDrawList& drawList, const ImVec2 min, const ImVec2 max,
                     const rts::core::render::UpdateMinimap& mm) {
        ImVec2 mapMin, mapMax;
        minimapInnerRect(min, max, mapMin, mapMax);
        const float spanX = mapMax.x - mapMin.x;
        const float spanY = mapMax.y - mapMin.y;
        drawList.AddRectFilled(mapMin, mapMax, IM_COL32(6, 14, 16, 255), 1.0f);

        // Terrain + fog: one cell per fog tile, tinted by explored/visible/unexplored.
        if (mm.fogW > 0 && mm.fogH > 0 &&
            static_cast<int>(mm.fog.size()) >= mm.fogW * mm.fogH) {
            const float cw = spanX / static_cast<float>(mm.fogW);
            const float ch = spanY / static_cast<float>(mm.fogH);
            for (int y = 0; y < mm.fogH; ++y) {
                for (int x = 0; x < mm.fogW; ++x) {
                    const uint8_t s = mm.fog[static_cast<std::size_t>(y) * mm.fogW + x];
                    ImU32 col;
                    if (s == 2)      col = IM_COL32(36, 92, 70, 255);   // visible terrain
                    else if (s == 1) col = IM_COL32(20, 48, 44, 255);   // explored (dim)
                    else             col = IM_COL32(8, 16, 18, 255);    // unexplored
                    const ImVec2 a{mapMin.x + x * cw, mapMin.y + y * ch};
                    drawList.AddRectFilled(a, {a.x + cw + 1.0f, a.y + ch + 1.0f}, col);
                }
            }
        }

        // Entity blips (player/enemy/resource); GameUIManager omits fogged enemies.
        for (const auto& dot : mm.dots) {
            const ImVec2 p{mapMin.x + dot.u * spanX, mapMin.y + dot.v * spanY};
            ImU32 col;
            switch (dot.team) {
                case 1:  col = IM_COL32(232, 86, 74, 255);  break;   // enemy red
                case 2:  col = IM_COL32(236, 198, 81, 255); break;   // neutral/resource gold
                default: col = IM_COL32(86, 200, 235, 255); break;   // player blue
            }
            drawList.AddRectFilled({p.x - 2.0f, p.y - 2.0f}, {p.x + 2.0f, p.y + 2.0f}, col);
        }

        // Camera viewport rectangle.
        if (mm.camW > 0.0f && mm.camH > 0.0f) {
            const ImVec2 vMin{mapMin.x + mm.camU * spanX, mapMin.y + mm.camV * spanY};
            const ImVec2 vMax{vMin.x + mm.camW * spanX, vMin.y + mm.camH * spanY};
            drawList.AddRect(vMin, vMax, IM_COL32(220, 240, 232, 235), 0.0f, 0, 1.5f);
        }

        drawList.AddRect(mapMin, mapMax, kPanelHigh, 1.0f, 0, 1.0f);
    }

    void drawPortrait(ImDrawList& drawList, const ImVec2 min, const ImVec2 max, const sf::Texture* avatar) {
        drawList.AddRectFilled(min, max, kPanelDark, 1.0f);
        drawList.AddRect(min, max, kPanelHigh, 1.0f, 0, 1.5f);

        if (avatar) {
            drawImage(drawList, avatar, min + ImVec2{10.0f, 10.0f}, max + ImVec2{-10.0f, -10.0f});
        } else {
            const ImVec2 center{(min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f};
            drawList.AddCircleFilled(center, 42.0f, IM_COL32(42, 98, 108, 255), 32);
            drawList.AddCircle(center, 50.0f, IM_COL32(123, 220, 215, 180), 32, 2.0f);
            drawList.AddText({center.x - 28.0f, center.y - 7.0f}, kTextMain, "HERO");
        }
    }

    // Green/amber/red by health fraction, for HP bars and portrait tints.
    ImU32 hpBarColor(const float ratio) {
        if (ratio > 0.6f) return IM_COL32(78, 200, 96, 255);
        if (ratio > 0.3f) return IM_COL32(236, 198, 81, 255);
        return IM_COL32(228, 86, 74, 255);
    }

    // A framed bar filled left-to-right by ratio (0..1).
    void drawStatBar(ImDrawList& drawList, const ImVec2 pos, const ImVec2 size,
                     const float ratio, const ImU32 color) {
        const ImVec2 max{pos.x + size.x, pos.y + size.y};
        drawList.AddRectFilled(pos, max, IM_COL32(12, 18, 22, 230), 2.0f);
        const float clamped = ratio < 0.0f ? 0.0f : (ratio > 1.0f ? 1.0f : ratio);
        drawList.AddRectFilled(pos, {pos.x + size.x * clamped, max.y}, color, 2.0f);
        drawList.AddRect(pos, max, IM_COL32(70, 126, 138, 210), 2.0f, 0, 1.0f);
    }

    // A label above a progress bar with a trailing percentage.
    void drawLabeledProgress(ImDrawList& drawList, const ImVec2 pos, const ImVec2 size,
                             const char* label, const float ratio, const ImU32 color) {
        drawList.AddText({pos.x, pos.y - 16.0f}, kTextDim, label);
        drawStatBar(drawList, pos, size, ratio, color);
        const int pct = static_cast<int>(std::lround((ratio < 0.0f ? 0.0f : (ratio > 1.0f ? 1.0f : ratio)) * 100.0f));
        const std::string text = std::to_string(pct) + "%";
        const ImVec2 ts = ImGui::CalcTextSize(text.c_str());
        drawList.AddText({pos.x + size.x - ts.x - 4.0f, pos.y + (size.y - ts.y) * 0.5f}, kTextMain, text.c_str());
    }

    // A small stat icon (or a colored swatch fallback when the texture is missing).
    void drawStatIcon(ImDrawList& drawList, const ImVec2 pos, const sf::Texture* icon, const ImU32 fallback) {
        const ImVec2 max{pos.x + 18.0f, pos.y + 18.0f};
        if (icon) {
            drawImage(drawList, icon, pos, max);
        } else {
            drawList.AddRectFilled(pos, max, fallback, 3.0f);
        }
    }

    // Tint for a selected element's portrait by its kind.
    ImU32 portraitKindColor(const rts::core::render::HudSelectionKind kind) {
        switch (kind) {
            case rts::core::render::HudSelectionKind::Worker:    return IM_COL32(78, 218, 148, 255);
            case rts::core::render::HudSelectionKind::CombatUnit:return IM_COL32(75, 196, 235, 255);
            case rts::core::render::HudSelectionKind::Building:  return IM_COL32(82, 145, 158, 255);
            case rts::core::render::HudSelectionKind::Resource:  return IM_COL32(236, 198, 81, 255);
            default:                                             return IM_COL32(120, 130, 134, 255);
        }
    }

    void drawStatusBar(ImDrawList& drawList, const ImVec2 min, const ImVec2 max, const float ratio, const ImU32 color, const char* label, const sf::Texture* base, const sf::Texture* fill) {
        if (base) {
            drawImage(drawList, base, min, max);
        } else {
            drawList.AddRectFilled(min, max, IM_COL32(7, 14, 18, 255), 1.0f);
        }

        const float clampedRatio = std::clamp(ratio, 0.0f, 1.0f);
        const ImVec2 fillMax{min.x + (max.x - min.x) * clampedRatio, max.y};
        if (fill) {
            drawList.AddImage(textureRef(*fill), min, fillMax, {0.0f, 0.0f}, {clampedRatio, 1.0f}, IM_COL32(255, 255, 255, 238));
        } else {
            drawList.AddRectFilled(min, fillMax, color, 1.0f);
        }

        drawList.AddRect(min, max, IM_COL32(110, 160, 162, 180), 1.0f);
        drawList.AddText({min.x + 8.0f, min.y + 3.0f}, kTextMain, label);
    }
}

namespace rts::platform::sfml {
    SfmlHudOverlay::SfmlHudOverlay(core::command::UICommandBus& uiBus)
        : m_uiBus(uiBus) {
    }

    SfmlHudOverlay::~SfmlHudOverlay() {
        shutdown();
    }

    void SfmlHudOverlay::render(
        sf::RenderWindow& window,
        const core::model::PlayerResourceState& resources,
        const core::render::UpdateHudSelection& selection,
        const core::render::UpdateMinimap& minimap) {
        if (!m_context) {
            initialize(window);
        }

        if (!m_context) {
            return;
        }

        ImGui::SetCurrentContext(m_context);
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        syncMouseButtons();
        ImGui::NewFrame();

        drawHud(ImGui::GetIO().DisplaySize, resources, selection, minimap);

        ImGui::Render();
        if (!window.setActive(true)) {
            return;
        }

        window.pushGLStates();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        window.popGLStates();
        window.resetGLStates();
    }

    void SfmlHudOverlay::initialize(sf::RenderWindow& window) {
        if (!window.setActive(true)) {
            return;
        }

        IMGUI_CHECKVERSION();
        m_context = ImGui::CreateContext();
        ImGui::SetCurrentContext(m_context);

        auto& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;

        applyStyle();

        const bool platformReady = ImGui_ImplWin32_InitForOpenGL(static_cast<void*>(window.getNativeHandle()));
        const bool rendererReady = ImGui_ImplOpenGL3_Init("#version 130");
        if (!platformReady || !rendererReady) {
            shutdown();
        }
    }

    void SfmlHudOverlay::shutdown() {
        if (!m_context) {
            return;
        }

        ImGui::SetCurrentContext(m_context);
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext(m_context);
        m_context = nullptr;
    }

    void SfmlHudOverlay::syncMouseButtons() {
        const std::array<bool, 3> pressed{
            (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0,
            (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0,
            (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0
        };

        auto& io = ImGui::GetIO();
        for (int i = 0; i < static_cast<int>(pressed.size()); ++i) {
            if (m_mouseButtons[i] != pressed[i]) {
                io.AddMouseButtonEvent(i, pressed[i]);
                m_mouseButtons[i] = pressed[i];
            }
        }
    }

    const sf::Texture* SfmlHudOverlay::texture(const std::string& relativePath) {
        if (const auto it = m_textures.find(relativePath); it != m_textures.end()) {
            return it->second.get();
        }

        auto loaded = std::make_unique<sf::Texture>();
        loaded->setSmooth(false);

        const std::filesystem::path path = tinySwordsRoot() / std::filesystem::path(relativePath);
        if (!loaded->loadFromFile(path.string())) {
            return nullptr;
        }

        const sf::Texture* result = loaded.get();
        m_textures.emplace(relativePath, std::move(loaded));
        return result;
    }

    const sf::Texture* SfmlHudOverlay::portraitTexture(const core::render::HudPortrait& p) {
        // Prefer the data-driven portrait configured in units.json / buildings.json.
        if (!p.iconPath.empty()) {
            if (const sf::Texture* tex = texture(p.iconPath)) {
                return tex;
            }
        }
        if (p.unitTypeId >= 0) {
            // One of the 25 Human Avatars, picked deterministically per unit type.
            const int idx = (p.unitTypeId % 25) + 1;
            const std::string num = (idx < 10 ? "0" : "") + std::to_string(idx);
            return texture("UI Elements/UI Elements/Human Avatars/Avatars_" + num + ".png");
        }
        if (p.buildingTypeId >= 0) {
            const char* color = (p.team == 2) ? "Red" : "Blue";  // TeamId::Enemy == 2
            // BuildingType: 0 = TownHall (Castle), others use a House.
            const char* file = (p.buildingTypeId == 0) ? "Castle.png" : "House1.png";
            return texture(std::string("Buildings/") + color + " Buildings/" + file);
        }
        return nullptr;
    }

    void SfmlHudOverlay::applyStyle() {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 0.0f;
        style.ChildRounding = 2.0f;
        style.FrameRounding = 2.0f;
        style.ScrollbarRounding = 2.0f;
        style.WindowBorderSize = 0.0f;
        style.FrameBorderSize = 1.0f;
        style.ItemSpacing = {8.0f, 8.0f};
        style.WindowPadding = {0.0f, 0.0f};

        ImVec4* colors = style.Colors;
        colors[ImGuiCol_WindowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        colors[ImGuiCol_Text] = ImVec4(0.82f, 0.95f, 0.92f, 1.0f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.48f, 0.65f, 0.64f, 1.0f);
        colors[ImGuiCol_Border] = ImVec4(0.35f, 0.65f, 0.68f, 0.72f);
    }

    void SfmlHudOverlay::drawHud(
        const ImVec2& displaySize,
        const core::model::PlayerResourceState& resources,
        const core::render::UpdateHudSelection& selection,
        const core::render::UpdateMinimap& minimap) {
        const float width = std::max(displaySize.x, 1280.0f);
        const float height = std::max(displaySize.y, 720.0f);
        // HUD ADJUST: bottomHeight changes the full lower HUD height; margin changes outer spacing.
        const float bottomHeight = std::clamp(height * 0.24f, 220.0f, 270.0f);
        const float margin = 16.0f;
        const float bottomY = height - bottomHeight;

        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoBackground;

        ImGui::SetNextWindowPos({0.0f, bottomY}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({width, bottomHeight}, ImGuiCond_Always);
        ImGui::Begin("##rts_star_command_hud", nullptr, flags);

        ImDrawList& drawList = *ImGui::GetWindowDrawList();
        const sf::Texture* woodTable = texture(kWoodTable);
        const sf::Texture* woodSlots = texture(kWoodSlots);
        const sf::Texture* banner = texture(kBanner);
        const sf::Texture* bannerSlots = texture(kBannerSlots);
        const sf::Texture* buttonRegular = texture(kButtonRegular);
        const sf::Texture* buttonPressed = texture(kButtonPressed);
        const sf::Texture* avatar = texture(kAvatar);
        const sf::Texture* swords = texture(kSwords);

        const ImVec2 consoleMin{0.0f, bottomY};
        const ImVec2 consoleMax{width, height};
        // HUD ADJUST: this is the full-width bottom console background png.
        drawList.AddRectFilled(consoleMin, consoleMax, kConsoleFill);
        drawList.AddLine({0.0f, bottomY}, {width, bottomY}, kPanelEdge, 2.0f);

        drawList.PushClipRectFullScreen();

        const ImVec2 resourceSize{142.0f, 34.0f};
        const float resourceTop = 12.0f;
        const float resourceStart = width - (resourceSize.x * 4.0f) - (12.0f * 3.0f) - margin;
        const std::string gold = formatNumber(resources.gold);
        const std::string wood = formatNumber(resources.wood);
        const std::string food = formatFood(resources);
        const std::string army = formatNumber(resources.army);
        // Population at/over capacity blocks new production: flag it red.
        const bool supplyCapped = resources.foodUsed >= resources.foodCapacity;
        // HUD ADJUST: top-right resource pills are positioned and spaced in this block.
        drawResourcePill(drawList, {resourceStart, resourceTop}, resourceSize, "Gold", gold.c_str(), kMineral, texture("UI Elements/UI Elements/Icons/Icon_01.png"));
        drawResourcePill(drawList, {resourceStart + 154.0f, resourceTop}, resourceSize, "Wood", wood.c_str(), kGas, texture("UI Elements/UI Elements/Icons/Icon_02.png"));
        drawResourcePill(drawList, {resourceStart + 308.0f, resourceTop}, resourceSize, "Food", food.c_str(), kWarning, texture("UI Elements/UI Elements/Icons/Icon_03.png"), nullptr, supplyCapped ? kDanger : kTextMain);
        drawResourcePill(drawList, {resourceStart + 462.0f, resourceTop}, resourceSize, "Army", army.c_str(), kPanelHigh, swords, &kBlueSwordIcon);

        const float miniWidth = std::clamp(width * 0.18f, 285.0f, 340.0f);
        const float commandWidth = std::clamp(width * 0.22f, 360.0f, 420.0f);
        // HUD ADJUST: mini/status/command section bounds are calculated here.
        const ImVec2 miniMin{margin, bottomY + margin};
        const ImVec2 miniMax{miniMin.x + miniWidth, height - margin};
        const ImVec2 commandMin{width - commandWidth - margin, bottomY + margin};
        const ImVec2 commandMax{width - margin, height - margin};
        const ImVec2 statusMin{miniMax.x + margin, bottomY + margin};
        const ImVec2 statusMax{commandMin.x - margin, height - margin};

        // HUD ADJUST: last two numbers are sourceEdge and targetEdge for each repeated png slice.
        drawPanelFrame(drawList, miniMin, miniMax, "MINI MAP", bannerSlots, 64.0f, 32.0f);
        drawPanelFrame(drawList, statusMin, statusMax, "SELECTION", woodTable, 128.0f, 56.0f);
        drawPanelFrame(drawList, commandMin, commandMax, "COMMAND", woodSlots, 64.0f, 32.0f);
        drawMiniMap(drawList, miniMin, miniMax, minimap);

        // Minimap interaction: a click maps to normalized world coords. Left button
        // recenters the camera; right button issues a world order there. An invisible
        // button over the inner map captures the click without blocking other input.
        {
            ImVec2 mapMin, mapMax;
            minimapInnerRect(miniMin, miniMax, mapMin, mapMax);
            ImGui::SetCursorScreenPos(mapMin);
            ImGui::InvisibleButton("##minimap", {mapMax.x - mapMin.x, mapMax.y - mapMin.y});
            if (ImGui::IsItemActive() || ImGui::IsItemHovered()) {
                const bool left = ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
                                  ImGui::IsMouseDragging(ImGuiMouseButton_Left);
                const bool right = ImGui::IsMouseClicked(ImGuiMouseButton_Right);
                if ((left || right) && ImGui::IsItemHovered()) {
                    const ImVec2 mp = ImGui::GetMousePos();
                    const float u = std::clamp((mp.x - mapMin.x) / std::max(1.0f, mapMax.x - mapMin.x), 0.0f, 1.0f);
                    const float v = std::clamp((mp.y - mapMin.y) / std::max(1.0f, mapMax.y - mapMin.y), 0.0f, 1.0f);
                    m_uiBus.push(std::make_unique<core::command::MinimapCommand>(u, v, right));
                }
            }
        }

        const ImVec2 portraitMin{statusMin.x + 18.0f, statusMin.y + 44.0f};
        const ImVec2 portraitMax{portraitMin.x + 132.0f, statusMax.y - 18.0f};
        // HUD ADJUST: selection portrait and unit text positions start here.
        const bool multiSelect = selection.selectedCount > 1;
        if (multiSelect) {
            // Multi-selection: a StarCraft-style grid of unit portraits (icon + HP bar).
            // Clicking one selects just that unit.
            const ImVec2 gridMin = portraitMin;
            const ImVec2 gridMax{statusMax.x - 18.0f, statusMax.y - 18.0f};
            const int count = static_cast<int>(selection.portraits.size());
            if (count > 0) {
                const int cols = count <= 6 ? 3 : (count <= 12 ? 4 : 6);
                const int rows = (count + cols - 1) / cols;
                const float gap = 4.0f;
                // Keep the multi-selection list compact: cells are square and no longer
                // stretch across the full status panel width.
                const float cellSize = std::floor(std::min({
                    44.0f,
                    (gridMax.x - gridMin.x - gap * (cols - 1)) / static_cast<float>(cols),
                    (gridMax.y - gridMin.y - gap * (rows - 1)) / static_cast<float>(rows)
                }));
                for (int i = 0; i < count; ++i) {
                    const auto& p = selection.portraits[i];
                    const int col = i % cols;
                    const int row = i / cols;
                    const ImVec2 cMin{gridMin.x + col * (cellSize + gap), gridMin.y + row * (cellSize + gap)};
                    if (cMin.y + cellSize > gridMax.y) break;  // clip overflow rows
                    const ImVec2 cMax{cMin.x + cellSize, cMin.y + cellSize};
                    ImGui::SetCursorScreenPos(cMin);
                    const std::string pid = "##portrait_" + std::to_string(i);
                    const bool clicked = ImGui::InvisibleButton(pid.c_str(), {cellSize, cellSize});
                    const bool hovered = ImGui::IsItemHovered();

                    drawList.AddRectFilled(cMin, cMax, kPanelDark, 2.0f);
                    if (const sf::Texture* icon = portraitTexture(p)) {
                        drawImage(drawList, icon, {cMin.x + 3.0f, cMin.y + 3.0f},
                                  {cMax.x - 3.0f, cMax.y - 8.0f});
                    }
                    drawList.AddRect(cMin, cMax,
                                     hovered ? IM_COL32(255, 255, 255, 255) : portraitKindColor(p.kind),
                                     2.0f, 0, hovered ? 2.0f : 1.5f);
                    const float ratio = std::clamp(p.hp01, 0.0f, 1.0f);
                    drawList.AddRectFilled({cMin.x + 2.0f, cMax.y - 6.0f},
                                           {cMin.x + 2.0f + (cellSize - 4.0f) * ratio, cMax.y - 2.0f},
                                           hpBarColor(ratio));
                    if (clicked) {
                        m_uiBus.push(std::make_unique<core::command::SelectEntityUICommand>(
                            p.entityIndex, p.entityGeneration));
                    }
                }
            }
        } else {
            // Single selection: unit avatar / building art for the large portrait.
            const sf::Texture* primaryArt = selection.portraits.empty()
                ? avatar : portraitTexture(selection.portraits.front());
            drawPortrait(drawList, portraitMin, portraitMax, primaryArt);
            // Single selection: an HP bar under the portrait.
            if (selection.hasPrimaryUnit && selection.maxHp > 0.0f) {
                const float ratio = std::clamp(selection.hp / selection.maxHp, 0.0f, 1.0f);
                drawStatBar(drawList, {portraitMin.x, portraitMax.y + 6.0f},
                            {portraitMax.x - portraitMin.x, 10.0f}, ratio, hpBarColor(ratio));
            }
        }

        if (!multiSelect) {
            const ImVec2 infoMin{portraitMax.x + 24.0f, statusMin.y + 48.0f};
            const std::string selectedCount = "Selected: " + std::to_string(selection.selectedCount) +
                (selection.selectedCount == 1 ? " unit" : " units");
            const std::string action = "Action: " + selection.action;
            const std::string hp = selection.hasPrimaryUnit
                ? "HP " + formatRounded(selection.hp) + " / " + formatRounded(selection.maxHp)
                : "HP -";
            const std::string position = selection.hasPrimaryUnit
                ? "Position " + formatPosition(selection.position)
                : "Position -";
            const std::string command = "Last command: " + m_lastCommand;

            drawList.AddText(infoMin, kTextMain, selection.primaryName.c_str());
            drawList.AddText({infoMin.x, infoMin.y + 28.0f}, kTextDim, selectedCount.c_str());
            drawList.AddText({infoMin.x, infoMin.y + 56.0f}, kTextDim, action.c_str());

            // Combat stats with attack/armor icons (Icon_04 = attack, Icon_05 = armor).
            const float statsY = infoMin.y + 84.0f;
            if (selection.hasCombatStats) {
                const sf::Texture* atkIcon = texture("UI Elements/UI Elements/Icons/Icon_04.png");
                const sf::Texture* armIcon = texture("UI Elements/UI Elements/Icons/Icon_05.png");
                const std::string atk = formatRounded(selection.attackDamage);
                const std::string arm = formatRounded(selection.armor);
                const std::string rng = "Rng " + formatRounded(selection.attackRange);
                drawStatIcon(drawList, {infoMin.x, statsY}, atkIcon, kWarning);
                drawList.AddText({infoMin.x + 24.0f, statsY + 2.0f}, kTextMain, atk.c_str());
                drawStatIcon(drawList, {infoMin.x + 78.0f, statsY}, armIcon, kMineral);
                drawList.AddText({infoMin.x + 102.0f, statsY + 2.0f}, kTextMain, arm.c_str());
                drawList.AddText({infoMin.x + 156.0f, statsY + 2.0f}, kTextDim, rng.c_str());
            } else {
                drawList.AddText({infoMin.x, statsY}, kTextDim, "Stats -");
            }

            drawList.AddText({infoMin.x, infoMin.y + 116.0f}, selection.hasPrimaryUnit ? kTextMain : kTextDim, hp.c_str());
            drawList.AddText({infoMin.x, infoMin.y + 148.0f}, kTextMain, position.c_str());
            drawList.AddText({infoMin.x, infoMin.y + 180.0f}, kWarning, command.c_str());

            if (selection.kind == core::render::HudSelectionKind::Building && selection.producesUnits) {
                constexpr float kCell = 32.0f;
                constexpr float kGap = 5.0f;
                constexpr int kMaxCells = 5;
                const float listWidth = kCell * kMaxCells + kGap * (kMaxCells - 1);
                const float listX = std::max(infoMin.x + 230.0f, statusMax.x - 18.0f - listWidth);
                const auto drawUnitCell = [&](const ImVec2 cellMin,
                                              const std::string& iconKey,
                                              const bool locked,
                                              const bool hovered,
                                              const float progress) {
                    const ImVec2 cellMax{cellMin.x + kCell, cellMin.y + kCell};
                    drawList.AddRectFilled(cellMin, cellMax, kPanelDark, 2.0f);
                    const auto* clip = core::data::DataRegistry::global().sprite(iconKey);
                    if (!clip) {
                        clip = core::data::DataRegistry::global().sprite("command.default");
                    }
                    if (clip) {
                        drawSpriteClip(
                            drawList,
                            texture(clip->texture),
                            *clip,
                            {cellMin.x + 3.0f, cellMin.y + 3.0f},
                            {cellMax.x - 3.0f, cellMax.y - 7.0f},
                            locked ? IM_COL32(140, 148, 150, 190) : IM_COL32_WHITE);
                    }
                    if (progress > 0.0f) {
                        const float ratio = std::clamp(progress, 0.0f, 1.0f);
                        drawList.AddRectFilled(
                            {cellMin.x + 2.0f, cellMax.y - 5.0f},
                            {cellMin.x + 2.0f + (kCell - 4.0f) * ratio, cellMax.y - 2.0f},
                            kWarning);
                    }
                    drawList.AddRect(
                        cellMin,
                        cellMax,
                        hovered ? IM_COL32(255, 255, 255, 255)
                                : (locked ? kTextDim : kPanelEdge),
                        2.0f,
                        0,
                        hovered ? 2.0f : 1.25f);
                };

                const ImVec2 producesPos{listX, infoMin.y + 84.0f};
                drawList.AddText({producesPos.x, producesPos.y - 17.0f}, kTextDim, "Produces");
                const int produceCount = std::min(static_cast<int>(selection.trainOptions.size()), kMaxCells);
                for (int i = 0; i < produceCount; ++i) {
                    const auto& opt = selection.trainOptions[i];
                    const ImVec2 cellMin{producesPos.x + i * (kCell + kGap), producesPos.y};
                    const bool locked = !selection.canProduce || opt.locked;
                    ImGui::SetCursorScreenPos(cellMin);
                    const std::string id = "##produce_icon_" + std::to_string(i);
                    const bool clicked = ImGui::InvisibleButton(id.c_str(), {kCell, kCell}) && !locked;
                    const bool hovered = ImGui::IsItemHovered() && !locked;
                    drawUnitCell(cellMin, opt.iconKey, locked, hovered, 0.0f);
                    if (clicked) {
                        m_lastCommand = opt.label;
                        m_uiBus.push(std::make_unique<core::command::TrainMenuSelectCommand>(opt.unitTypeId));
                    }
                }

                if (!selection.trainQueue.empty()) {
                    const ImVec2 queuePos{listX, infoMin.y + 144.0f};
                    const std::string queueLabel = "Queue " + std::to_string(selection.trainQueue.size())
                        + "/" + std::to_string(kMaxCells);
                    drawList.AddText({queuePos.x, queuePos.y - 17.0f}, kTextDim, queueLabel.c_str());
                    const int queueCount = std::min(static_cast<int>(selection.trainQueue.size()), kMaxCells);
                    for (int i = 0; i < queueCount; ++i) {
                        const auto& item = selection.trainQueue[i];
                        const ImVec2 cellMin{queuePos.x + i * (kCell + kGap), queuePos.y};
                        drawUnitCell(cellMin, item.iconKey, false, false, i == 0 ? item.progress01 : 0.0f);
                    }
                }
            }

            // Building progress overlays: construction first, otherwise training progress.
            if (selection.kind == core::render::HudSelectionKind::Building) {
                const ImVec2 barPos{infoMin.x, infoMin.y + 208.0f};
                const ImVec2 barSize{220.0f, 12.0f};
                if (selection.underConstruction) {
                    drawLabeledProgress(drawList, barPos, barSize, "Building",
                                        selection.buildProgress01, kGas);
                } else if (selection.trainQueueCount > 0) {
                    const std::string lbl = "Training x" + std::to_string(selection.trainQueueCount);
                    drawLabeledProgress(drawList, barPos, barSize, lbl.c_str(),
                                        selection.trainProgress01, kWarning);
                }
            }
        }

        // Command buttons depend on what is selected, so a barracks offers Train
        // while a worker offers Build/Gather and combat units offer Attack/Patrol.
        using core::command::GameplayInputAction;
        using core::render::HudSelectionKind;
        // The build submenu only applies to a worker selection.
        if (selection.kind != HudSelectionKind::Worker) {
            m_buildMenuOpen = false;
        }
        std::vector<HudCommandButton> commands;
        switch (selection.kind) {
            case HudSelectionKind::Building:
                // A producing building shows one button per trainable unit; each is
                // locked (greyed) while the building is not ready (incomplete/tech) or
                // the unit is unaffordable / prerequisite-locked.
                if (selection.producesUnits) {
                    for (const auto& opt : selection.trainOptions) {
                        HudCommandButton btn;
                        btn.label = opt.label;
                        btn.locked = !selection.canProduce || opt.locked;
                        btn.kind = HudCommandButton::Kind::SelectTrain;
                        btn.payloadId = opt.unitTypeId;
                        btn.iconKey = opt.iconKey;
                        commands.push_back(btn);
                    }
                }
                commands.push_back(commandButton(
                    "Cancel", GameplayInputAction::CancelProduction, "C", "command.cancel"));
                break;
            case HudSelectionKind::Worker:
                if (m_buildMenuOpen) {
                    // Build submenu: one button per constructible structure + Cancel.
                    for (const auto& opt : selection.buildOptions) {
                        HudCommandButton btn;
                        btn.label = opt.label;
                        btn.locked = opt.locked;
                        btn.kind = HudCommandButton::Kind::SelectBuild;
                        btn.payloadId = opt.buildingTypeId;
                        btn.iconKey = opt.iconKey;
                        commands.push_back(btn);
                    }
                    HudCommandButton cancel;
                    cancel.label = "Cancel";
                    cancel.hotkey = "Esc";
                    cancel.kind = HudCommandButton::Kind::CloseBuildMenu;
                    cancel.iconKey = "command.cancel";
                    commands.push_back(cancel);
                } else {
                    commands = {
                        commandButton("Move", GameplayInputAction::Move, "M", "command.move"),
                        commandButton("Stop", GameplayInputAction::Stop, "S", "command.stop"),
                        commandButton("Hold", GameplayInputAction::HoldPosition, "H", "command.hold"),
                        commandButton("Gather", GameplayInputAction::Gather, "G", "command.gather"),
                        commandButton("Build", GameplayInputAction::Build, "B", "command.build",
                                      HudCommandButton::Kind::OpenBuildMenu),
                        commandButton("A-Move", GameplayInputAction::AttackMove, "A", "command.attack_move")
                    };
                }
                break;
            case HudSelectionKind::CombatUnit:
                commands = {
                    commandButton("Move", GameplayInputAction::Move, "M", "command.move"),
                    commandButton("Stop", GameplayInputAction::Stop, "S", "command.stop"),
                    commandButton("Hold", GameplayInputAction::HoldPosition, "H", "command.hold"),
                    commandButton("A-Move", GameplayInputAction::AttackMove, "A", "command.attack_move"),
                    commandButton("Patrol", GameplayInputAction::Patrol, "P", "command.patrol")
                };
                break;
            case HudSelectionKind::Resource:
            case HudSelectionKind::None:
                break;  // nothing actionable selected
        }

        const float gridTop = commandMin.y + 44.0f;
        const float cellGap = 8.0f;
        // HUD ADJUST: command button grid starts here; tweak gridTop, cellGap, or 3-column math.
        const float cellW = (commandMax.x - commandMin.x - 28.0f - cellGap * 2.0f) / 3.0f;
        const float cellH = (commandMax.y - gridTop - 16.0f - cellGap * 2.0f) / 3.0f;

        for (int i = 0; i < static_cast<int>(commands.size()); ++i) {
            const int col = i % 3;
            const int row = i / 3;
            const ImVec2 pos{
                commandMin.x + 14.0f + col * (cellW + cellGap),
                gridTop + row * (cellH + cellGap)
            };
            ImGui::SetCursorScreenPos(pos);
            const HudCommandButton& command = commands[i];
            const std::string id = "##command_" + std::to_string(i);
            // Locked buttons render but swallow no input (the click is ignored below).
            const bool clicked = ImGui::InvisibleButton(id.c_str(), {cellW, cellH}) && !command.locked;
            const bool active = ImGui::IsItemActive() && !command.locked;
            const bool hovered = ImGui::IsItemHovered() && !command.locked;

            const ImU32 buttonTint = command.locked
                ? IM_COL32(120, 130, 134, 200)
                : (hovered ? IM_COL32(255, 255, 255, 255) : IM_COL32(225, 235, 240, 245));
            drawImage(drawList, active ? buttonPressed : buttonRegular, pos, {pos.x + cellW, pos.y + cellH}, buttonTint);
            const std::string iconKey = command.iconKey.empty() ? "command.default" : command.iconKey;
            const auto* clip = core::data::DataRegistry::global().sprite(iconKey);
            if (!clip && iconKey != "command.default") {
                clip = core::data::DataRegistry::global().sprite("command.default");
            }
            if (clip) {
                const sf::Texture* icon = texture(clip->texture);
                const ImVec2 iconMin{pos.x + cellW * 0.5f - 15.0f, pos.y + 10.0f};
                drawSpriteClip(
                    drawList,
                    icon,
                    *clip,
                    iconMin,
                    {iconMin.x + 30.0f, iconMin.y + 30.0f},
                    command.locked ? IM_COL32(140, 148, 150, 200) : IM_COL32_WHITE);
            }

            const ImU32 labelColor = command.locked ? kTextDim : kTextMain;
            const ImVec2 textSize = ImGui::CalcTextSize(command.label.c_str());
            drawList.AddText({pos.x + (cellW - textSize.x) * 0.5f, pos.y + cellH - textSize.y - 10.0f}, labelColor, command.label.c_str());
            // Hotkey hint in the top-left corner so the shortcut is discoverable.
            drawList.AddText({pos.x + 6.0f, pos.y + 4.0f}, command.locked ? kTextDim : kWarning, command.hotkey);

            if (clicked) {
                m_lastCommand = command.label;
                switch (command.kind) {
                    case HudCommandButton::Kind::Action:
                        // Consumed next frame and translated into a LogicCommand.
                        m_uiBus.push(std::make_unique<core::command::GameplayInputCommand>(command.action));
                        break;
                    case HudCommandButton::Kind::OpenBuildMenu:
                        m_buildMenuOpen = true;
                        break;
                    case HudCommandButton::Kind::SelectBuild:
                        m_uiBus.push(std::make_unique<core::command::BuildMenuSelectCommand>(command.payloadId));
                        m_buildMenuOpen = false;
                        break;
                    case HudCommandButton::Kind::CloseBuildMenu:
                        m_buildMenuOpen = false;
                        break;
                    case HudCommandButton::Kind::SelectTrain:
                        m_uiBus.push(std::make_unique<core::command::TrainMenuSelectCommand>(command.payloadId));
                        break;
                }
            }
        }

        drawList.PopClipRect();
        ImGui::End();
    }
}
