#include "core/viewmodel/UnitViewModel.hpp"
#include "core/model/Unit.hpp"

namespace rts::core::viewmodel {
    UnitViewModel::UnitViewModel(std::shared_ptr<model::Unit> unit)
        : m_unit(unit) {
        if (auto u = m_unit.lock()) {
            m_position = u->getPosition();
            m_action = u->getAction();
            m_hpRatio = u->getHp() / u->getMaxHp();
        }
    }

    void UnitViewModel::update() {
        auto unit = m_unit.lock();
        if (!unit)
            return;

        m_position = unit->getPosition();
        m_action = unit->getAction();
        m_hpRatio = unit->getHp() / unit->getMaxHp();
    }

    // ===== IViewModel =====

    bool UnitViewModel::visible() const {
        return m_visible;
    }

    void UnitViewModel::setVisible(bool v) {
        m_visible = v;
    }

    const char *UnitViewModel::name() const {
        return "Unit";
    }

    bool UnitViewModel::expired() const {
        return m_unit.expired();
    }

    const void *UnitViewModel::modelPtr() const {
        if (auto u = m_unit.lock()) {
            return u.get();
        }
        return nullptr;
    }

    void UnitViewModel::buildRenderCommands(render::RenderQueue &out) const {
        auto unit = m_unit.lock();
        if (!unit) return;

        model::Vector2D pos = unit->getPosition();
        if (unit->state().selected) {
            out.emplace(
                core::render::RenderLayer::World,
                9,
                render::DrawCircle{
                    .cx = pos.x,
                    .cy = pos.y,
                    .radius = 34.0f,
                    .border_color = 0xFF45F6B2,
                    .color = 0x3045F6B2
                });
        }

        // Trimmed unit sprites are bottom-centered so the model position stays at the feet.
        out.emplace(
            core::render::RenderLayer::World,
            10,
            render::DrawSprite{
                .x = pos.x - 48.0f,
                .y = pos.y - 96.0f,
                .w = 96.0f,
                .h = 96.0f,
                .textureId = 1,
                .sourceX = 0,
                .sourceY = 0,
                .sourceW = 192,
                .sourceH = 192,
                .trimTransparent = true
            });
    }
}
