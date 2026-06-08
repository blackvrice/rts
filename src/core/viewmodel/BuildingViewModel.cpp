#include "core/viewmodel/BuildingViewModel.hpp"
#include "core/model/Building.hpp"
#include "core/data/DataRegistry.hpp"
#include "core/render/RenderCommand.hpp"
#include "core/render/RenderQueue.hpp"

#include <string>

namespace {
    const char* buildingIdStr(rts::core::model::BuildingType type) {
        return type == rts::core::model::BuildingType::TownHall ? "town_hall" : "barracks";
    }

    std::string buildingSpriteKey(rts::core::model::BuildingType type, int teamId) {
        const std::string team = teamId == rts::core::model::TeamId::Enemy ? "red" : "blue";
        return std::string("building.") + buildingIdStr(type) + "." + team;
    }
}

namespace rts::core::viewmodel {

    BuildingViewModel::BuildingViewModel(std::shared_ptr<model::Building> building)
        : m_building(std::move(building)) {}

    bool BuildingViewModel::expired() const {
        return !m_building || m_building->getAction() == model::ActionType::Dead;
    }

    const void* BuildingViewModel::modelPtr() const {
        return m_building.get();
    }

    void BuildingViewModel::buildRenderCommands(render::RenderQueue& out) const {
        if (!m_building || m_building->getAction() == model::ActionType::Dead) return;

        const auto pos = m_building->getPosition();

        // Selection ring
        if (m_building->state().selected) {
            out.emplace(render::RenderLayer::World, 8,
                render::DrawCircle{
                    .cx = pos.x, .cy = pos.y, .radius = 60.f,
                    .border_color = 0xFF00FFFFu,
                    .color        = 0x2000FFFFu
                });
        }

        // Building sprite resolved from data (data/animations.json).
        if (const auto* clip = core::data::DataRegistry::global().sprite(
                buildingSpriteKey(m_building->buildingType(), m_building->getTeamId()))) {
            out.emplace(render::RenderLayer::World, 5,
                render::DrawSprite{
                    .x = pos.x - clip->anchorX, .y = pos.y - clip->anchorY,
                    .w = clip->displayW,         .h = clip->displayH,
                    .textureId    = 0,
                    .texturePath  = clip->texture,
                    .sourceX = clip->sourceX, .sourceY = clip->sourceY,
                    .sourceW = clip->sourceW, .sourceH = clip->sourceH,
                    .frameCount   = clip->frameCount,
                    .framesPerSecond = clip->fps,
                    .trimTransparent = clip->trim
                });
        }

        // HP bar background
        const float barW = 84.f;
        const float barH = 7.f;
        const float barX = pos.x - barW * 0.5f;
        const float barY = pos.y - 98.f;
        out.emplace(render::RenderLayer::World, 9,
            render::DrawRect{
                .rect = model::Rect{
                    model::Vector2D{barX, barY},
                    model::Vector2D{barX + barW, barY + barH}
                },
                .border_color = 0xFF000000u,
                .color        = 0xFF222222u
            });

        // HP bar fill
        const float ratio = m_building->getMaxHp() > 0.f
            ? m_building->getHp() / m_building->getMaxHp() : 0.f;
        const uint32_t fillColor = ratio > 0.5f ? 0xFF44EE44u
                                 : ratio > 0.25f ? 0xFFFFAA00u
                                 :                 0xFFEE2222u;
        if (ratio > 0.f) {
            out.emplace(render::RenderLayer::World, 10,
                render::DrawRect{
                    .rect = model::Rect{
                        model::Vector2D{barX, barY},
                        model::Vector2D{barX + barW * ratio, barY + barH}
                    },
                    .border_color = 0x00000000u,
                    .color        = fillColor
                });
        }

        // Train progress bar (shown when queue is non-empty)
        if (m_building->trainQueueSize() > 0) {
            const float progW = barW * m_building->trainProgress();
            const float progY = barY + barH + 2.f;
            out.emplace(render::RenderLayer::World, 10,
                render::DrawRect{
                    .rect = model::Rect{
                        model::Vector2D{barX, progY},
                        model::Vector2D{barX + barW, progY + 4.f}
                    },
                    .border_color = 0xFF000000u,
                    .color        = 0xFF1A1A1Au
                });
            if (progW > 0.f) {
                out.emplace(render::RenderLayer::World, 11,
                    render::DrawRect{
                        .rect = model::Rect{
                            model::Vector2D{barX, progY},
                            model::Vector2D{barX + progW, progY + 4.f}
                        },
                        .border_color = 0x00000000u,
                        .color        = 0xFFFFD700u
                    });
            }
        }
    }

} // namespace rts::core::viewmodel
