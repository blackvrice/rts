#include "core/viewmodel/ResourceNodeViewModel.hpp"
#include "core/model/ResourceNode.hpp"
#include "core/data/DataRegistry.hpp"
#include "core/render/RenderCommand.hpp"
#include "core/render/RenderQueue.hpp"
#include <algorithm>
#include <cmath>
#include <string>

namespace {
    // Gold renders as the "Gold Stone" pile: a fuller pile shows a higher stage
    // (1..6) and the pile shrinks toward stage 1 as it depletes. Each stage is a
    // separate clip keyed "resource.gold.<stage>" in data/animations.json.
    constexpr int kGoldStoneStages = 6;

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

        // 스프라이트 (data/animations.json에서 해석)
        std::string spriteKey;
        if (isGold) {
            const int stage = goldStoneStage(m_node->remaining(), m_node->totalAmount());
            spriteKey = "resource.gold." + std::to_string(stage);
        } else {
            spriteKey = "resource.wood";
        }
        if (const auto* clip = core::data::DataRegistry::global().sprite(spriteKey)) {
            out.emplace(core::render::RenderLayer::World, 5,
                core::render::DrawSprite{
                    .x = pos.x - clip->anchorX, .y = pos.y - clip->anchorY,
                    .w = clip->displayW,         .h = clip->displayH,
                    .textureId = 0,
                    .texturePath = clip->texture,
                    .sourceX = clip->sourceX, .sourceY = clip->sourceY,
                    .sourceW = clip->sourceW, .sourceH = clip->sourceH,
                    .frameCount = clip->frameCount,
                    .framesPerSecond = clip->fps,
                    .trimTransparent = clip->trim
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
