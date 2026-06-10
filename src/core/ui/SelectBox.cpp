//
// Created by black on 26. 1. 4..
//

#include "core/ui/SelectBox.hpp"

#include "core/manager/CameraManager.hpp"

namespace rts::core::ui {
    SelectBox::SelectBox(command::LogicCommandBus &bus, manager::CameraManager& camera,
                         const bool& additive, const bool& sameType)
        : m_additive(additive), m_sameType(sameType) {
        // 1️⃣ 드래그 시작
        MouseDown += [this](const model::Vector2D &p) {
            m_visible = true;
            m_start = p;
            m_end = p;
        };

        // 2️⃣ 드래그 중
        MouseMove += [this](const model::Vector2D &p) {
            if (!m_visible) return;
            m_end = p;
        };

        // 3️⃣ 드래그 종료
        MouseUp += [this, &bus, &camera](const model::Vector2D &p) {
            if (!m_visible) return;

            m_end = p;
            m_visible = false;

            bus.push(std::make_unique<command::SelectCommand>(
                camera.screenToWorld(m_start),
                camera.screenToWorld(m_end),
                m_additive,
                m_sameType
            ));
        };
    }

    void SelectBox::update() {
    }

    void SelectBox::buildRenderCommands(render::RenderQueue &q) const {
        if (!m_visible) return;
        // 2️⃣ 테두리
        q.emplace(
            render::RenderLayer::UI,
            10,
            render::DrawRect{
                model::Rect(m_start, m_end),
            0x87CEEBFF,
                0x87CEEB40
            }
        );
    }
}
