#include "game/game/systems/SelectionSystem.hpp"

#include <algorithm>
#include <limits>
#include <vector>

#include <core/model/IGameElement.hpp>
#include <core/model/Rect.hpp>
#include <core/model/Unit.hpp>
#include <core/world/GameWorld.hpp>

namespace rts::core::manager {
    namespace {
        // A select area smaller than this counts as a click (point pick) rather
        // than a drag box.
        constexpr float kClickThreshold = 6.0f;
        // Click/point picks snap to the nearest element within this world radius.
        constexpr float kClickPickRadius = 30.0f;

        float distanceSq(const model::Vector2D& a, const model::Vector2D& b) {
            const float dx = a.x - b.x;
            const float dy = a.y - b.y;
            return dx * dx + dy * dy;
        }

        bool isLiveUnit(const std::shared_ptr<model::IElement>& element) {
            return std::dynamic_pointer_cast<model::Unit>(element) != nullptr;
        }
    }

    void SelectionSystem::clear() {
        for (const auto& weak : m_selectedElements) {
            if (auto element = weak.lock()) {
                element->setSelected(false);
            }
        }

        m_selectedElements.clear();
    }

    void SelectionSystem::selectElement(model::IGameElement& element) {
        element.setSelected(true);
        m_selectedElements.push_back(element.weak_from_this());
    }

    void SelectionSystem::addSelectedElement(model::IGameElement& element) {
        selectElement(element);
    }

    void SelectionSystem::applySelection(
        const std::vector<std::shared_ptr<model::IGameElement>>& chosen,
        const bool additive,
        const bool toggle) {
        if (!additive) {
            clear();
        }

        const auto alreadySelected = [this](const model::IGameElement* e) {
            for (const auto& weak : m_selectedElements) {
                if (weak.lock().get() == e) return true;
            }
            return false;
        };

        for (const auto& element : chosen) {
            if (!element) {
                continue;
            }
            if (alreadySelected(element.get())) {
                if (additive && toggle) {
                    // Shift-click an already-selected element to deselect it.
                    element->setSelected(false);
                    std::erase_if(m_selectedElements, [&](const auto& weak) {
                        return weak.lock().get() == element.get();
                    });
                }
                continue;
            }
            if (m_selectedElements.size() >= kMaxSelection) {
                break;
            }
            element->setSelected(true);
            m_selectedElements.push_back(element);
        }
    }

    void SelectionSystem::selectInArea(const world::GameWorld& world, const model::Rect& area,
                                       const bool additive) {
        const bool isClick =
            area.width() < kClickThreshold && area.height() < kClickThreshold;

        if (isClick) {
            // Point pick: nearest element under the cursor, preferring the
            // player's own units, then any unit, then a building/resource.
            const model::Vector2D point {
                (area.left() + area.right()) * 0.5f,
                (area.top() + area.bottom()) * 0.5f
            };
            std::shared_ptr<model::IGameElement> bestPlayerUnit, bestUnit, bestOther;
            float dPlayer = std::numeric_limits<float>::max();
            float dUnit = dPlayer;
            float dOther = dPlayer;
            const float pickSq = kClickPickRadius * kClickPickRadius;

            for (const auto& element : world.getElements()) {
                auto ge = std::dynamic_pointer_cast<model::IGameElement>(element);
                if (!ge || ge->getAction() == model::ActionType::Dead) {
                    continue;
                }
                const float d = distanceSq(ge->getPosition(), point);
                if (d > pickSq) {
                    continue;
                }
                if (isLiveUnit(element)) {
                    if (d < dUnit) { dUnit = d; bestUnit = ge; }
                    if (ge->getTeamId() == model::TeamId::Player && d < dPlayer) {
                        dPlayer = d;
                        bestPlayerUnit = ge;
                    }
                } else if (d < dOther) {
                    dOther = d;
                    bestOther = ge;
                }
            }

            auto target = bestPlayerUnit ? bestPlayerUnit
                        : bestUnit ? bestUnit
                        : bestOther;
            applySelection({ target }, additive, /*toggle=*/true);
            return;
        }

        // Drag box: units outrank buildings/resources, and the player's own units
        // outrank everything else.
        std::vector<std::shared_ptr<model::IGameElement>> units, playerUnits, others;
        for (const auto& element : world.getElements()) {
            auto ge = std::dynamic_pointer_cast<model::IGameElement>(element);
            if (!ge || ge->getAction() == model::ActionType::Dead ||
                !area.contains(ge->getPosition())) {
                continue;
            }
            if (isLiveUnit(element)) {
                units.push_back(ge);
                if (ge->getTeamId() == model::TeamId::Player) {
                    playerUnits.push_back(ge);
                }
            } else {
                others.push_back(ge);
            }
        }

        const auto& chosen = !playerUnits.empty() ? playerUnits
                           : !units.empty() ? units
                           : others;
        applySelection(chosen, additive, /*toggle=*/false);
    }

    void SelectionSystem::selectSameType(const world::GameWorld& world,
                                         const model::Vector2D& point, const bool additive) {
        // Resolve the unit under the cursor (prefer the player's own).
        std::shared_ptr<model::Unit> picked, pickedPlayer;
        float dAny = std::numeric_limits<float>::max();
        float dPlayer = dAny;
        const float pickSq = kClickPickRadius * kClickPickRadius;

        for (const auto& element : world.getElements()) {
            auto unit = std::dynamic_pointer_cast<model::Unit>(element);
            if (!unit || unit->getAction() == model::ActionType::Dead) {
                continue;
            }
            const float d = distanceSq(unit->getPosition(), point);
            if (d > pickSq) {
                continue;
            }
            if (d < dAny) { dAny = d; picked = unit; }
            if (unit->getTeamId() == model::TeamId::Player && d < dPlayer) {
                dPlayer = d;
                pickedPlayer = unit;
            }
        }

        const auto anchor = pickedPlayer ? pickedPlayer : picked;
        if (!anchor) {
            // Nothing to match: behave like a normal click.
            const model::Rect clickArea{ point, point };
            selectInArea(world, clickArea, additive);
            return;
        }

        const auto type = anchor->unitType();
        const int team = anchor->getTeamId();
        std::vector<std::shared_ptr<model::IGameElement>> matches;
        for (const auto& element : world.getElements()) {
            auto unit = std::dynamic_pointer_cast<model::Unit>(element);
            if (unit && unit->getAction() != model::ActionType::Dead &&
                unit->unitType() == type && unit->getTeamId() == team) {
                matches.push_back(unit);
            }
        }
        applySelection(matches, additive, /*toggle=*/false);
    }

    void SelectionSystem::replaceSelected(SelectedList selected) {
        clear();

        m_selectedElements = std::move(selected);
        for (const auto& weak : m_selectedElements) {
            if (auto element = weak.lock()) {
                element->setSelected(true);
            }
        }
    }

    const SelectionSystem::SelectedList& SelectionSystem::selected() const noexcept {
        return m_selectedElements;
    }
}
