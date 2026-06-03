#include "platform/sfml/SfmlHudOverlay.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Texture.hpp>

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

    constexpr SourceRect kBlueSwordIcon{0.0f, 0.0f, 96.0f, 128.0f};

    std::filesystem::path tinySwordsRoot() {
        return std::filesystem::path(rts::platform::sfml::TinySwordsRoot);
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
    void drawNineSlice(ImDrawList& drawList, const sf::Texture* texture, const ImVec2 min, const ImVec2 max, const float sourceEdge, const float targetEdge, const ImU32 tint = IM_COL32_WHITE) {
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

        drawImageRect(drawList, texture, SourceRect{sx[0], sy[0], sourceEdge, sourceEdge}, {dx[0], dy[0]}, {dx[1], dy[1]}, tint);
        drawImageRect(drawList, texture, SourceRect{sx[2], sy[0], sourceEdge, sourceEdge}, {dx[2], dy[0]}, {dx[3], dy[1]}, tint);
        drawImageRect(drawList, texture, SourceRect{sx[0], sy[2], sourceEdge, sourceEdge}, {dx[0], dy[2]}, {dx[1], dy[3]}, tint);
        drawImageRect(drawList, texture, SourceRect{sx[2], sy[2], sourceEdge, sourceEdge}, {dx[2], dy[2]}, {dx[3], dy[3]}, tint);

        if (sourceCenterW > 0.0f) {
            const ImVec2 horizontalTileSize{sourceCenterW * scale, edge};
            drawTiledImageRect(drawList, texture, SourceRect{sx[1], sy[0], sourceCenterW, sourceEdge}, {dx[1], dy[0]}, {dx[2], dy[1]}, horizontalTileSize, tint);
            drawTiledImageRect(drawList, texture, SourceRect{sx[1], sy[2], sourceCenterW, sourceEdge}, {dx[1], dy[2]}, {dx[2], dy[3]}, horizontalTileSize, tint);
        }

        if (sourceCenterH > 0.0f) {
            const ImVec2 verticalTileSize{edge, sourceCenterH * scale};
            drawTiledImageRect(drawList, texture, SourceRect{sx[0], sy[1], sourceEdge, sourceCenterH}, {dx[0], dy[1]}, {dx[1], dy[2]}, verticalTileSize, tint);
            drawTiledImageRect(drawList, texture, SourceRect{sx[2], sy[1], sourceEdge, sourceCenterH}, {dx[2], dy[1]}, {dx[3], dy[2]}, verticalTileSize, tint);
        }

        if (sourceCenterW > 0.0f && sourceCenterH > 0.0f) {
            drawTiledImageRect(
                drawList,
                texture,
                SourceRect{sx[1], sy[1], sourceCenterW, sourceCenterH},
                {dx[1], dy[1]},
                {dx[2], dy[2]},
                {sourceCenterW * scale, sourceCenterH * scale},
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

    void drawResourcePill(ImDrawList& drawList, const ImVec2 min, const ImVec2 size, const char* label, const char* value, const ImU32 color, const sf::Texture* icon, const SourceRect* iconSource = nullptr) {
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
        drawList.AddText({max.x - valueSize.x - 12.0f, min.y + 7.0f}, kTextMain, value);
    }

    void drawMiniMap(ImDrawList& drawList, const ImVec2 min, const ImVec2 max) {
        const ImVec2 mapMin{min.x + 14.0f, min.y + 38.0f};
        const ImVec2 mapMax{max.x - 14.0f, max.y - 14.0f};
        drawList.AddRectFilled(mapMin, mapMax, IM_COL32(8, 25, 24, 255), 1.0f);

        constexpr int grid = 8;
        const float cellW = (mapMax.x - mapMin.x) / grid;
        const float cellH = (mapMax.y - mapMin.y) / grid;
        for (int y = 0; y < grid; ++y) {
            for (int x = 0; x < grid; ++x) {
                const bool ridge = (x + y) % 3 == 0;
                const ImU32 tile = ridge ? IM_COL32(30, 74, 58, 255) : IM_COL32(18, 52, 56, 255);
                const ImVec2 a{mapMin.x + x * cellW + 1.0f, mapMin.y + y * cellH + 1.0f};
                const ImVec2 b{a.x + cellW - 2.0f, a.y + cellH - 2.0f};
                drawList.AddRectFilled(a, b, tile);
            }
        }

        drawList.AddRect({mapMin.x + 42.0f, mapMin.y + 32.0f}, {mapMin.x + 132.0f, mapMin.y + 88.0f}, IM_COL32(210, 238, 230, 220), 0.0f, 0, 2.0f);
        drawList.AddCircleFilled({mapMin.x + 72.0f, mapMin.y + 62.0f}, 4.0f, kMineral, 16);
        drawList.AddCircleFilled({mapMin.x + 178.0f, mapMin.y + 126.0f}, 4.0f, kWarning, 16);
        drawList.AddCircleFilled({mapMin.x + 216.0f, mapMin.y + 50.0f}, 3.0f, kGas, 16);
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
    SfmlHudOverlay::~SfmlHudOverlay() {
        shutdown();
    }

    void SfmlHudOverlay::render(sf::RenderWindow& window) {
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

        drawHud(ImGui::GetIO().DisplaySize);

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

    void SfmlHudOverlay::drawHud(const ImVec2& displaySize) {
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

        ImGui::SetNextWindowPos({0.0f, 0.0f}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({width, height}, ImGuiCond_Always);
        ImGui::Begin("##rts_star_command_hud", nullptr, flags);

        ImDrawList& drawList = *ImGui::GetWindowDrawList();
        const sf::Texture* woodTable = texture(kWoodTable);
        const sf::Texture* woodSlots = texture(kWoodSlots);
        const sf::Texture* banner = texture(kBanner);
        const sf::Texture* bannerSlots = texture(kBannerSlots);
        const sf::Texture* buttonRegular = texture(kButtonRegular);
        const sf::Texture* buttonPressed = texture(kButtonPressed);
        const sf::Texture* barBase = texture(kBarBase);
        const sf::Texture* barFill = texture(kBarFill);
        const sf::Texture* avatar = texture(kAvatar);
        const sf::Texture* swords = texture(kSwords);

        const ImVec2 consoleMin{0.0f, bottomY};
        const ImVec2 consoleMax{width, height};
        // HUD ADJUST: this is the full-width bottom console background png.
        if (woodTable) {
            drawNineSlice(drawList, woodTable, consoleMin, consoleMax, 128.0f, 72.0f, IM_COL32(255, 255, 255, 238));
            drawList.AddRectFilled(consoleMin, consoleMax, IM_COL32(20, 12, 7, 96));
        } else {
            drawList.AddRectFilled(consoleMin, consoleMax, kConsoleFill);
        }
        drawList.AddLine({0.0f, bottomY}, {width, bottomY}, kPanelEdge, 2.0f);

        const ImVec2 resourceSize{142.0f, 34.0f};
        const float resourceTop = 12.0f;
        const float resourceStart = width - (resourceSize.x * 4.0f) - (12.0f * 3.0f) - margin;
        // HUD ADJUST: top-right resource pills are positioned and spaced in this block.
        drawResourcePill(drawList, {resourceStart, resourceTop}, resourceSize, "Gold", "1,500", kMineral, texture("UI Elements/UI Elements/Icons/Icon_01.png"));
        drawResourcePill(drawList, {resourceStart + 154.0f, resourceTop}, resourceSize, "Wood", "520", kGas, texture("UI Elements/UI Elements/Icons/Icon_02.png"));
        drawResourcePill(drawList, {resourceStart + 308.0f, resourceTop}, resourceSize, "Food", "24/32", kWarning, texture("UI Elements/UI Elements/Icons/Icon_03.png"));
        drawResourcePill(drawList, {resourceStart + 462.0f, resourceTop}, resourceSize, "Army", "142", kPanelHigh, swords, &kBlueSwordIcon);

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
        drawPanelFrame(drawList, statusMin, statusMax, "SELECTION", banner, 128.0f, 56.0f);
        drawPanelFrame(drawList, commandMin, commandMax, "COMMAND", woodSlots, 64.0f, 32.0f);
        drawMiniMap(drawList, miniMin, miniMax);

        const ImVec2 portraitMin{statusMin.x + 18.0f, statusMin.y + 44.0f};
        const ImVec2 portraitMax{portraitMin.x + 132.0f, statusMax.y - 18.0f};
        // HUD ADJUST: selection portrait and unit text positions start here.
        drawPortrait(drawList, portraitMin, portraitMax, avatar);

        const ImVec2 infoMin{portraitMax.x + 24.0f, statusMin.y + 48.0f};
        drawList.AddText(infoMin, kTextMain, "Tiny Swords Vanguard");
        drawList.AddText({infoMin.x, infoMin.y + 28.0f}, kTextDim, "Selected: 6 units");
        drawList.AddText({infoMin.x, infoMin.y + 56.0f}, kTextDim, "Armor 1   Range 5   Damage 6");
        drawStatusBar(drawList, {infoMin.x, infoMin.y + 92.0f}, {statusMax.x - 22.0f, infoMin.y + 114.0f}, 0.82f, IM_COL32(75, 205, 116, 255), "HP 492 / 600", barBase, barFill);
        drawStatusBar(drawList, {infoMin.x, infoMin.y + 124.0f}, {statusMax.x - 22.0f, infoMin.y + 146.0f}, 0.46f, IM_COL32(73, 153, 232, 255), "Morale 46%", barBase, barFill);
        drawList.AddText({infoMin.x, infoMin.y + 164.0f}, kWarning, ("Last command: " + m_lastCommand).c_str());

        const std::array<const char*, 9> commands{
            "Move", "Stop", "Hold",
            "Attack", "Patrol", "Gather",
            "Build", "Repair", "Cancel"
        };

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
            const std::string id = std::string("##command_") + commands[i];
            const bool clicked = ImGui::InvisibleButton(id.c_str(), {cellW, cellH});
            const bool active = ImGui::IsItemActive();
            const bool hovered = ImGui::IsItemHovered();

            drawImage(drawList, active ? buttonPressed : buttonRegular, pos, {pos.x + cellW, pos.y + cellH}, hovered ? IM_COL32(255, 255, 255, 255) : IM_COL32(225, 235, 240, 245));
            if (const sf::Texture* icon = texture("UI Elements/UI Elements/Icons/Icon_0" + std::to_string((i % 9) + 1) + ".png")) {
                const ImVec2 iconMin{pos.x + cellW * 0.5f - 15.0f, pos.y + 10.0f};
                drawImage(drawList, icon, iconMin, {iconMin.x + 30.0f, iconMin.y + 30.0f});
            }

            const ImVec2 textSize = ImGui::CalcTextSize(commands[i]);
            drawList.AddText({pos.x + (cellW - textSize.x) * 0.5f, pos.y + cellH - textSize.y - 10.0f}, kTextMain, commands[i]);

            if (clicked) {
                m_lastCommand = commands[i];
            }
        }

        ImGui::End();
    }
}
