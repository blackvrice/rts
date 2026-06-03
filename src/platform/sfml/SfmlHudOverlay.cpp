#include "platform/sfml/SfmlHudOverlay.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <algorithm>
#include <array>
#include <cmath>

#include <SFML/Graphics/RenderWindow.hpp>

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_win32.h"

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

    ImVec2 operator+(const ImVec2& a, const ImVec2& b) {
        return {a.x + b.x, a.y + b.y};
    }

    void drawPanelFrame(ImDrawList& drawList, const ImVec2 min, const ImVec2 max, const char* title) {
        drawList.AddRectFilled(min, max, kPanelFill, 2.0f);
        drawList.AddRect(min, max, kPanelEdge, 2.0f, 0, 2.0f);
        drawList.AddRect(min + ImVec2{4.0f, 4.0f}, max + ImVec2{-4.0f, -4.0f}, kPanelHigh, 1.0f, 0, 1.0f);

        const ImVec2 titlePos{min.x + 12.0f, min.y + 8.0f};
        drawList.AddText(titlePos, kTextDim, title);
        drawList.AddLine({min.x + 8.0f, min.y + 30.0f}, {max.x - 8.0f, min.y + 30.0f}, IM_COL32(70, 120, 132, 180), 1.0f);
    }

    void drawResourcePill(ImDrawList& drawList, const ImVec2 min, const ImVec2 size, const char* label, const char* value, const ImU32 color) {
        const ImVec2 max{min.x + size.x, min.y + size.y};
        drawList.AddRectFilled(min, max, IM_COL32(8, 16, 22, 232), 2.0f);
        drawList.AddRect(min, max, IM_COL32(70, 126, 138, 210), 2.0f, 0, 1.0f);
        drawList.AddCircleFilled({min.x + 18.0f, min.y + size.y * 0.5f}, 6.0f, color, 16);
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

    void drawPortrait(ImDrawList& drawList, const ImVec2 min, const ImVec2 max) {
        drawList.AddRectFilled(min, max, kPanelDark, 1.0f);
        drawList.AddRect(min, max, kPanelHigh, 1.0f, 0, 1.5f);

        const ImVec2 center{(min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f};
        drawList.AddCircleFilled(center, 42.0f, IM_COL32(42, 98, 108, 255), 32);
        drawList.AddCircle(center, 50.0f, IM_COL32(123, 220, 215, 180), 32, 2.0f);
        drawList.AddText({center.x - 28.0f, center.y - 7.0f}, kTextMain, "MARINE");
    }

    void drawStatusBar(ImDrawList& drawList, const ImVec2 min, const ImVec2 max, const float ratio, const ImU32 color, const char* label) {
        drawList.AddRectFilled(min, max, IM_COL32(7, 14, 18, 255), 1.0f);
        drawList.AddRectFilled(min, {min.x + (max.x - min.x) * std::clamp(ratio, 0.0f, 1.0f), max.y}, color, 1.0f);
        drawList.AddRect(min, max, IM_COL32(110, 160, 162, 180), 1.0f);
        drawList.AddText({min.x + 8.0f, min.y + 3.0f}, kTextMain, label);
    }

    void pushCommandButtonStyle() {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.09f, 0.19f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.14f, 0.35f, 0.42f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35f, 0.55f, 0.48f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.45f, 0.78f, 0.80f, 0.72f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.82f, 0.95f, 0.92f, 1.0f));
    }

    void popCommandButtonStyle() {
        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar(2);
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
        const ImVec2 consoleMin{0.0f, bottomY};
        const ImVec2 consoleMax{width, height};
        drawList.AddRectFilled(consoleMin, consoleMax, kConsoleFill);
        drawList.AddLine({0.0f, bottomY}, {width, bottomY}, kPanelEdge, 2.0f);

        const ImVec2 resourceSize{142.0f, 34.0f};
        const float resourceTop = 12.0f;
        const float resourceStart = width - (resourceSize.x * 4.0f) - (12.0f * 3.0f) - margin;
        drawResourcePill(drawList, {resourceStart, resourceTop}, resourceSize, "Minerals", "1,500", kMineral);
        drawResourcePill(drawList, {resourceStart + 154.0f, resourceTop}, resourceSize, "Gas", "520", kGas);
        drawResourcePill(drawList, {resourceStart + 308.0f, resourceTop}, resourceSize, "Supply", "24/32", kWarning);
        drawResourcePill(drawList, {resourceStart + 462.0f, resourceTop}, resourceSize, "APM", "142", kPanelHigh);

        const float miniWidth = std::clamp(width * 0.18f, 285.0f, 340.0f);
        const float commandWidth = std::clamp(width * 0.22f, 360.0f, 420.0f);
        const ImVec2 miniMin{margin, bottomY + margin};
        const ImVec2 miniMax{miniMin.x + miniWidth, height - margin};
        const ImVec2 commandMin{width - commandWidth - margin, bottomY + margin};
        const ImVec2 commandMax{width - margin, height - margin};
        const ImVec2 statusMin{miniMax.x + margin, bottomY + margin};
        const ImVec2 statusMax{commandMin.x - margin, height - margin};

        drawPanelFrame(drawList, miniMin, miniMax, "MINI MAP");
        drawPanelFrame(drawList, statusMin, statusMax, "SELECTION");
        drawPanelFrame(drawList, commandMin, commandMax, "COMMAND");
        drawMiniMap(drawList, miniMin, miniMax);

        const ImVec2 portraitMin{statusMin.x + 18.0f, statusMin.y + 44.0f};
        const ImVec2 portraitMax{portraitMin.x + 132.0f, statusMax.y - 18.0f};
        drawPortrait(drawList, portraitMin, portraitMax);

        const ImVec2 infoMin{portraitMax.x + 24.0f, statusMin.y + 48.0f};
        drawList.AddText(infoMin, kTextMain, "Terran Infantry Squad");
        drawList.AddText({infoMin.x, infoMin.y + 28.0f}, kTextDim, "Selected: 6 units");
        drawList.AddText({infoMin.x, infoMin.y + 56.0f}, kTextDim, "Armor 1   Range 5   Damage 6");
        drawStatusBar(drawList, {infoMin.x, infoMin.y + 92.0f}, {statusMax.x - 22.0f, infoMin.y + 114.0f}, 0.82f, IM_COL32(75, 205, 116, 255), "HP 492 / 600");
        drawStatusBar(drawList, {infoMin.x, infoMin.y + 124.0f}, {statusMax.x - 22.0f, infoMin.y + 146.0f}, 0.46f, IM_COL32(73, 153, 232, 255), "Energy 46%");
        drawList.AddText({infoMin.x, infoMin.y + 164.0f}, kWarning, ("Last command: " + m_lastCommand).c_str());

        const std::array<const char*, 9> commands{
            "Move", "Stop", "Hold",
            "Attack", "Patrol", "Gather",
            "Build", "Repair", "Cancel"
        };

        const float gridTop = commandMin.y + 44.0f;
        const float cellGap = 8.0f;
        const float cellW = (commandMax.x - commandMin.x - 28.0f - cellGap * 2.0f) / 3.0f;
        const float cellH = (commandMax.y - gridTop - 16.0f - cellGap * 2.0f) / 3.0f;

        pushCommandButtonStyle();
        for (int i = 0; i < static_cast<int>(commands.size()); ++i) {
            const int col = i % 3;
            const int row = i / 3;
            const ImVec2 pos{
                commandMin.x + 14.0f + col * (cellW + cellGap),
                gridTop + row * (cellH + cellGap)
            };
            ImGui::SetCursorScreenPos(pos);
            if (ImGui::Button(commands[i], {cellW, cellH})) {
                m_lastCommand = commands[i];
            }
        }
        popCommandButtonStyle();

        ImGui::End();
    }
}
