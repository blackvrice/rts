#include "core/viewmodel/UnitViewModel.hpp"
#include "core/model/Unit.hpp"
#include "core/data/DataRegistry.hpp"

#include <string>

namespace {
    const char* actionKey(const rts::core::model::ActionType action) {
        using A = rts::core::model::ActionType;
        switch (action) {
            case A::Move:   return "move";
            case A::Attack: return "attack";
            case A::Hold:   return "hold";
            default:        return "idle";
        }
    }

    const char* unitIdStr(const rts::UnitType type) {
        switch (type) {
            case rts::UnitType::Warrior: return "warrior";
            case rts::UnitType::Archer:  return "archer";
            case rts::UnitType::Marine:  return "marine";
            case rts::UnitType::Worker:  return "worker";
        }
        return "warrior";
    }
}

namespace rts::core::viewmodel {
    UnitViewModel::UnitViewModel(std::shared_ptr<model::Unit> unit)
        : m_unit(unit) {
        if (auto u = m_unit.lock()) {
            m_position = u->getPosition();
            m_action = u->getAnimationAction();
            m_hpRatio = u->getHp() / u->getMaxHp();
        }
    }

    void UnitViewModel::update() {
        auto unit = m_unit.lock();
        if (!unit)
            return;

        m_position = unit->getPosition();
        m_action = unit->getAnimationAction();
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
        if (unit->getAction() == model::ActionType::Dead) return;

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

        // Resolve the sprite clip from data (data/animations.json), falling back
        // to the unit's idle clip when the action-specific one is missing.
        auto& registry = core::data::DataRegistry::global();
        const std::string set = registry.unitSpriteSet(unitIdStr(unit->unitType()));
        const std::string team = unit->getTeamId() == model::TeamId::Enemy ? "red" : "blue";
        const std::string base = "unit." + set + "." + team + ".";
        const core::data::SpriteClip* clip = registry.sprite(base + actionKey(m_action));
        if (!clip) clip = registry.sprite(base + "idle");
        if (!clip) return;

        // Trimmed unit sprites are bottom-centered so the model position stays at the feet.
        out.emplace(
            core::render::RenderLayer::World,
            10,
            render::DrawSprite{
                .x = pos.x - clip->anchorX,
                .y = pos.y - clip->anchorY,
                .w = clip->displayW,
                .h = clip->displayH,
                .textureId = 0,
                .texturePath = clip->texture,
                .sourceX = clip->sourceX,
                .sourceY = clip->sourceY,
                .sourceW = clip->sourceW,
                .sourceH = clip->sourceH,
                .frameCount = clip->frameCount,
                .framesPerSecond = clip->fps,
                .trimTransparent = clip->trim,
                .showInHud = unit->state().selected
            });
    }
}
