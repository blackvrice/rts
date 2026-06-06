#include "core/viewmodel/ResourceNodeViewModel.hpp"
#include "core/model/ResourceNode.hpp"
#include "core/render/RenderCommand.hpp"
#include "core/render/RenderQueue.hpp"
#include <string>

namespace {
    constexpr int kGoldTextureId = 200;
    constexpr int kWoodTextureId = 201;
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

        const auto pos   = m_node->getPosition();
        const int texId  = (m_node->type() == model::ResourceNode::ResourceType::Gold)
                         ? kGoldTextureId : kWoodTextureId;

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
        out.emplace(core::render::RenderLayer::World, 5,
            core::render::DrawSprite{
                .x = pos.x - 32.f, .y = pos.y - 48.f,
                .w = 64.f,         .h = 64.f,
                .textureId = texId,
                .frameCount = 1, .framesPerSecond = 0.f
            });

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
