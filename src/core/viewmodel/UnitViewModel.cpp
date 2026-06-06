#include "core/viewmodel/UnitViewModel.hpp"
#include "core/model/Unit.hpp"

namespace {
    // Texture id layout shared with SfmlRenderManager:
    //   Warrior: Blue base 1 (idle/run/attack/guard = +0..+3), Red base 11
    //   Pawn:    Blue base 21 (idle/run/interact),             Red base 31
    constexpr int kBlueWarriorBase = 1;
    constexpr int kRedWarriorBase = 11;
    constexpr int kBluePawnBase = 21;
    constexpr int kRedPawnBase = 31;

    struct UnitSpriteClip {
        int textureId;
        int frameCount;
        float framesPerSecond;
    };

    UnitSpriteClip spriteClipFor(const rts::UnitType unitType,
                                 const int teamId,
                                 const rts::core::model::ActionType action) {
        using rts::core::model::ActionType;
        const bool isEnemy = teamId == rts::core::model::TeamId::Enemy;

        if (unitType == rts::UnitType::Worker) {
            // Pawn sheets: Idle 8 frames, Run 6, Interact (used for work/attack) 3.
            // Pawn has no guard sheet, so Hold falls back to Idle.
            const int base = isEnemy ? kRedPawnBase : kBluePawnBase;
            switch (action) {
                case ActionType::Move:
                    return {base + 1, 6, 10.0f};
                case ActionType::Attack:
                    return {base + 2, 3, 8.0f};
                default:
                    return {base + 0, 8, 6.0f};
            }
        }

        // Warrior sheets cover all combat unit types for now.
        const int base = isEnemy ? kRedWarriorBase : kBlueWarriorBase;
        switch (action) {
            case ActionType::Move:
                return {base + 1, 6, 10.0f};
            case ActionType::Attack:
                return {base + 2, 4, 8.0f};
            case ActionType::Hold:
                return {base + 3, 6, 6.0f};
            default:
                return {base + 0, 8, 6.0f};
        }
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

        const UnitSpriteClip clip = spriteClipFor(unit->unitType(), unit->getTeamId(), m_action);

        // Trimmed unit sprites are bottom-centered so the model position stays at the feet.
        out.emplace(
            core::render::RenderLayer::World,
            10,
            render::DrawSprite{
                .x = pos.x - 48.0f,
                .y = pos.y - 96.0f,
                .w = 96.0f,
                .h = 96.0f,
                .textureId = clip.textureId,
                .sourceX = 0,
                .sourceY = 0,
                .sourceW = 192,
                .sourceH = 192,
                .frameCount = clip.frameCount,
                .framesPerSecond = clip.framesPerSecond,
                .trimTransparent = true,
                .showInHud = unit->state().selected
            });
    }
}
