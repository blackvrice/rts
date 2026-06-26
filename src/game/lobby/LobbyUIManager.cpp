//
// Created by black on 26. 1. 1..
//

#include "game/lobby/LobbyUIManager.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

#include "core/app/SessionContext.hpp"
#include "core/command/CommandRouterBase.hpp"
#include "core/command/LogicCommand.hpp"
#include "core/command/LogicCommandBus.hpp"
#include "core/command/UICommand.hpp"
#include "core/manager/CameraManager.hpp"
#include "core/render/RenderCommand.hpp"
#include "core/render/RenderQueue.hpp"

namespace rts::core::manager {
    namespace {
        // 0xRRGGBBAA (SFML color order).
        constexpr std::uint32_t kBg        = 0x10141Affu;
        constexpr std::uint32_t kTitle     = 0xE6F2FFFFu;
        constexpr std::uint32_t kSubtitle  = 0x8FA6B4FFu;
        constexpr std::uint32_t kBtnFill   = 0x244C46FFu;
        constexpr std::uint32_t kBtnHover  = 0x3A7E72FFu;
        constexpr std::uint32_t kBtnBorder = 0x8FE0D8FFu;
        constexpr std::uint32_t kBtnText   = 0xEAF6F2FFu;

        constexpr int kMaxReplayRows = 10;  // most-recent replays shown in the list
    }

    LobbyUIManager::LobbyUIManager(command::UICommandRouter& router, command::LogicCommandBus& logicBus,
                                   core::render::RenderQueue& renderQueue, CameraManager& camera)
        : IUIManager(router, logicBus, renderQueue), m_camera(camera) {

        router.on<command::MouseMoveCommand>(
            [this](const command::MouseMoveCommand& cmd) { m_mouse = cmd.position(); });

        router.on<command::MouseLeftPressedCommand>(
            [this](const command::MouseLeftPressedCommand& cmd) {
                const auto& p = cmd.position();
                if (m_view == View::Menu) {
                    if (startButton().rect.contains(p)) {
                        // Fresh match: make sure no stale replay is queued, then record.
                        core::app::SessionContext::instance().takeReplayToPlay();
                        m_logicBus.push(std::make_unique<command::SceneChangeCommand>("game"));
                    } else if (replaysButton().rect.contains(p)) {
                        refreshReplayList();
                        m_view = View::Replays;
                    }
                    return;
                }

                // Replay list view.
                if (backButton().rect.contains(p)) {
                    m_view = View::Menu;
                    return;
                }
                for (const auto& row : replayListButtons()) {
                    if (row.rect.contains(p)) {
                        core::app::SessionContext::instance().setReplayToPlay(row.replayPath);
                        m_logicBus.push(std::make_unique<command::SceneChangeCommand>("game"));
                        return;
                    }
                }
            });
    }

    LobbyUIManager::MenuButton LobbyUIManager::startButton() const {
        const auto& vp = m_camera.viewportSize();
        const float cx = vp.x * 0.5f;
        const float cy = vp.y * 0.52f;
        const float w = 380.0f;
        const float h = 88.0f;
        return MenuButton {
            core::model::Rect{ { cx - w * 0.5f, cy - h * 0.5f }, { cx + w * 0.5f, cy + h * 0.5f } },
            "START GAME", "game", ""
        };
    }

    LobbyUIManager::MenuButton LobbyUIManager::replaysButton() const {
        const auto& vp = m_camera.viewportSize();
        const float cx = vp.x * 0.5f;
        const float cy = vp.y * 0.52f + 112.0f;
        const float w = 380.0f;
        const float h = 88.0f;
        return MenuButton {
            core::model::Rect{ { cx - w * 0.5f, cy - h * 0.5f }, { cx + w * 0.5f, cy + h * 0.5f } },
            "REPLAYS", "", ""
        };
    }

    LobbyUIManager::MenuButton LobbyUIManager::backButton() const {
        const auto& vp = m_camera.viewportSize();
        const float w = 200.0f;
        const float h = 60.0f;
        return MenuButton {
            core::model::Rect{ { vp.x * 0.5f - w * 0.5f, vp.y * 0.86f - h * 0.5f },
                               { vp.x * 0.5f + w * 0.5f, vp.y * 0.86f + h * 0.5f } },
            "BACK", "", ""
        };
    }

    std::vector<LobbyUIManager::MenuButton> LobbyUIManager::replayListButtons() const {
        std::vector<MenuButton> rows;
        const auto& vp = m_camera.viewportSize();
        const float w = 560.0f;
        const float h = 52.0f;
        const float gap = 10.0f;
        const float cx = vp.x * 0.5f;
        const float top = vp.y * 0.30f;
        for (std::size_t i = 0; i < m_replayFiles.size() && i < kMaxReplayRows; ++i) {
            const float y = top + static_cast<float>(i) * (h + gap);
            const std::string name = std::filesystem::path(m_replayFiles[i]).stem().string();
            rows.push_back(MenuButton {
                core::model::Rect{ { cx - w * 0.5f, y }, { cx + w * 0.5f, y + h } },
                name, "game", m_replayFiles[i]
            });
        }
        return rows;
    }

    void LobbyUIManager::refreshReplayList() {
        m_replayFiles.clear();
        std::error_code ec;
        const std::string dir = core::app::SessionContext::replaysDir();
        for (std::filesystem::directory_iterator it(dir, ec), end; it != end; it.increment(ec)) {
            if (ec) break;
            if (it->is_regular_file(ec) && it->path().extension() == ".json") {
                m_replayFiles.push_back(it->path().string());
            }
        }
        // Timestamped names sort lexicographically; reverse for newest-first.
        std::sort(m_replayFiles.begin(), m_replayFiles.end(), std::greater<>());
    }

    void LobbyUIManager::update() {
    }

    void LobbyUIManager::render() {
        if (m_view == View::Menu) {
            renderMenu();
        } else {
            renderReplays();
        }
    }

    void LobbyUIManager::renderMenu() {
        const auto& vp = m_camera.viewportSize();
        const float cx = vp.x * 0.5f;
        using core::render::RenderLayer;

        // DrawText is left-anchored; offset by an estimated width to pseudo-center.
        const auto centeredX = [cx](const std::string& s, int size) {
            return cx - static_cast<float>(s.size()) * static_cast<float>(size) * 0.30f;
        };

        // Full-screen background.
        m_renderQueue.emplace(RenderLayer::UI, -100,
            core::render::DrawRect{
                .rect = core::model::Rect{ { 0.0f, 0.0f }, { vp.x, vp.y } },
                .border_color = 0x00000000u,
                .color = kBg
            });

        // Title + subtitle.
        const std::string title = "RTS";
        const std::string subtitle = "Real-Time Strategy  -  Vertical Slice";
        m_renderQueue.emplace(RenderLayer::UI, -80,
            core::render::DrawText{
                .pos = { centeredX(title, 88), vp.y * 0.22f },
                .color = kTitle, .fontId = 1u, .size = 88, .text = title
            });
        m_renderQueue.emplace(RenderLayer::UI, -80,
            core::render::DrawText{
                .pos = { centeredX(subtitle, 22), vp.y * 0.22f + 104.0f },
                .color = kSubtitle, .fontId = 1u, .size = 22, .text = subtitle
            });

        // Menu buttons (hover-highlighted).
        const auto drawButton = [&](const MenuButton& btn) {
            const bool hover = btn.rect.contains(m_mouse);
            m_renderQueue.emplace(RenderLayer::UI, -90,
                core::render::DrawRect{
                    .rect = btn.rect,
                    .border_color = kBtnBorder,
                    .color = hover ? kBtnHover : kBtnFill
                });
            m_renderQueue.emplace(RenderLayer::UI, -80,
                core::render::DrawText{
                    .pos = { centeredX(btn.label, 34),
                             btn.rect.top() + (btn.rect.height() - 34.0f) * 0.5f },
                    .color = kBtnText, .fontId = 1u, .size = 34, .text = btn.label
                });
        };
        drawButton(startButton());
        drawButton(replaysButton());

        // Footer hint.
        const std::string hint = "START GAME to play  -  REPLAYS to review past matches";
        m_renderQueue.emplace(RenderLayer::UI, -80,
            core::render::DrawText{
                .pos = { centeredX(hint, 20), vp.y * 0.84f },
                .color = kSubtitle, .fontId = 1u, .size = 20, .text = hint
            });
    }

    void LobbyUIManager::renderReplays() {
        const auto& vp = m_camera.viewportSize();
        const float cx = vp.x * 0.5f;
        using core::render::RenderLayer;

        const auto centeredX = [cx](const std::string& s, int size) {
            return cx - static_cast<float>(s.size()) * static_cast<float>(size) * 0.30f;
        };

        // Full-screen background.
        m_renderQueue.emplace(RenderLayer::UI, -100,
            core::render::DrawRect{
                .rect = core::model::Rect{ { 0.0f, 0.0f }, { vp.x, vp.y } },
                .border_color = 0x00000000u,
                .color = kBg
            });

        const std::string title = "REPLAYS";
        m_renderQueue.emplace(RenderLayer::UI, -80,
            core::render::DrawText{
                .pos = { centeredX(title, 56), vp.y * 0.16f },
                .color = kTitle, .fontId = 1u, .size = 56, .text = title
            });

        const auto rows = replayListButtons();
        if (rows.empty()) {
            const std::string empty = "No replays yet - finish or leave a match to save one.";
            m_renderQueue.emplace(RenderLayer::UI, -80,
                core::render::DrawText{
                    .pos = { centeredX(empty, 22), vp.y * 0.40f },
                    .color = kSubtitle, .fontId = 1u, .size = 22, .text = empty
                });
        } else {
            for (const auto& row : rows) {
                const bool hover = row.rect.contains(m_mouse);
                m_renderQueue.emplace(RenderLayer::UI, -90,
                    core::render::DrawRect{
                        .rect = row.rect,
                        .border_color = kBtnBorder,
                        .color = hover ? kBtnHover : kBtnFill
                    });
                m_renderQueue.emplace(RenderLayer::UI, -80,
                    core::render::DrawText{
                        .pos = { row.rect.left() + 20.0f,
                                 row.rect.top() + (row.rect.height() - 24.0f) * 0.5f },
                        .color = kBtnText, .fontId = 1u, .size = 24, .text = row.label
                    });
            }
        }

        // Back button.
        const auto back = backButton();
        const bool backHover = back.rect.contains(m_mouse);
        m_renderQueue.emplace(RenderLayer::UI, -90,
            core::render::DrawRect{
                .rect = back.rect,
                .border_color = kBtnBorder,
                .color = backHover ? kBtnHover : kBtnFill
            });
        m_renderQueue.emplace(RenderLayer::UI, -80,
            core::render::DrawText{
                .pos = { centeredX(back.label, 28),
                         back.rect.top() + (back.rect.height() - 28.0f) * 0.5f },
                .color = kBtnText, .fontId = 1u, .size = 28, .text = back.label
            });
    }

    void LobbyUIManager::syncWithWorld() {
    }
}
