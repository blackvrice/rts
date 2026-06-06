#include "core/viewmodel/ResourceNodeViewModel.hpp"
#include "core/model/ResourceNode.hpp"
#include "core/render/RenderCommand.hpp"
#include "core/render/RenderQueue.hpp"
#include <algorithm>
#include <cmath>
#include <string>

namespace {
    // Gold renders as the "Gold Stone" pile: stage 1..6 maps to texture ids
    // 210..215, each a 6-frame highlight sheet (128px frames) animated by default.
    constexpr int kGoldStoneHighlightBase = 210;
    constexpr int kGoldStoneStages = 6;
    constexpr int kGoldStoneFrame = 128;
    constexpr int kGoldStoneFrameCount = 6;
    constexpr float kGoldStoneFps = 8.0f;
    constexpr int kWoodTextureId = 201;

    // Fuller piles show a higher stage; the pile shrinks toward stage 1 as it
    // depletes. Depleted nodes are not rendered at all.
    int goldStoneStage(int remaining, int total) {
        if (total <= 0) return 1;
        const float ratio = static_cast<float>(remaining) / static_cast<float>(total);
        const int stage = static_cast<int>(std::ceil(ratio * kGoldStoneStages));
        return std::clamp(stage, 1, kGoldStoneStages);
    }
}

namespace rts::core::viewmodel {
    ResourceNodeViewModel::ResourceNodeViewModel(std::shared_ptr<model::ResourceNode> node)
        : m_node(std::move(node)) {}

    bool ResourceNodeViewModel::expired() const {
        return !m_node || m_node->isDepleted();
    }

    const void* ResourceNodeViewModel::modelPtr() const {
        return m_node.get();
    }

    void ResourceNodeViewModel::buildRenderCommands(render::RenderQueue& out) const {
        if (!m_node || m_node->isDepleted()) return;

        const auto pos    = m_node->getPosition();
        const bool isGold = m_node->type() == model::ResourceNode::ResourceType::Gold;

        // 선택 링
        if (m_node->state().selected) {
            out.emplace(core::render::RenderLayer::World, 8,
                core::render::DrawCircle{
                    .cx = pos.x, .cy = pos.y, .radius = 38.f,
                    .border_color = 0xFFFFD700u,
                    .color        = 0x20FFD700u
                });
        }

        // 스프라이트
        if (isGold) {
            const int stage = goldStoneStage(m_node->remaining(), m_node->totalAmount());
            out.emplace(core::render::RenderLayer::World, 5,
                core::render::DrawSprite{
                    .x = pos.x - 32.f, .y = pos.y - 48.f,
                    .w = 64.f,         .h = 64.f,
                    .textureId = kGoldStoneHighlightBase + (stage - 1),
                    .sourceX = 0, .sourceY = 0,
                    .sourceW = kGoldStoneFrame, .sourceH = kGoldStoneFrame,
                    .frameCount = kGoldStoneFrameCount,
                    .framesPerSecond = kGoldStoneFps
                });
        } else {
            out.emplace(core::render::RenderLayer::World, 5,
                core::render::DrawSprite{
                    .x = pos.x - 32.f, .y = pos.y - 48.f,
                    .w = 64.f,         .h = 64.f,
                    .textureId = kWoodTextureId,
                    .frameCount = 1, .framesPerSecond = 0.f
                });
        }

        // 남은 자원 텍스트
        const std::string label = std::to_string(m_node->remaining());
        out.emplace(core::render::RenderLayer::World, 6,
            core::render::DrawText{
                .pos = {pos.x - 12.f, pos.y - 56.f},
                .color = (m_node->type() == model::ResourceNode::ResourceType::Gold)
                       ? 0xFFFFD700u : 0xFF80FF80u,
                .fontId = 1u,
                .size = 14,
                .text = label
            });
    }
}
