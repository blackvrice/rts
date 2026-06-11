#include "core/world/WorldRuntimeServices.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>

#include "core/model/IGameElement.hpp"
#include "core/world/GameWorld.hpp"

namespace rts::core::world {
    namespace {
        struct SpatialCell {
            int x {};
            int y {};

            bool operator==(const SpatialCell& other) const noexcept {
                return x == other.x && y == other.y;
            }
        };

        struct SpatialCellHash {
            std::size_t operator()(const SpatialCell& cell) const noexcept {
                const std::uint64_t x = static_cast<std::uint32_t>(cell.x);
                const std::uint64_t y = static_cast<std::uint32_t>(cell.y);
                return std::hash<std::uint64_t>{}((x << 32) ^ y);
            }
        };

        struct RuntimeState {
            std::uint64_t nextSerial { 1 };
            std::vector<FeedbackEvent> feedback;
            std::vector<ActiveEffect> effects;
            std::unordered_map<SpatialCell, std::vector<std::weak_ptr<model::IGameElement>>, SpatialCellHash> spatial;
            float cellSize { 256.0f };
        };

        std::unordered_map<const GameWorld*, RuntimeState> g_runtime;
        const std::vector<FeedbackEvent> g_emptyFeedback;
        const std::vector<ActiveEffect> g_emptyEffects;

        RuntimeState& stateFor(const GameWorld& world) {
            return g_runtime[&world];
        }

        SpatialCell cellFor(const RuntimeState& state, const model::Vector2D& p) {
            const float size = state.cellSize > 0.0f ? state.cellSize : 256.0f;
            return {
                static_cast<int>(std::floor(p.x / size)),
                static_cast<int>(std::floor(p.y / size))
            };
        }

        float distanceSq(const model::Vector2D& a, const model::Vector2D& b) {
            const float dx = a.x - b.x;
            const float dy = a.y - b.y;
            return dx * dx + dy * dy;
        }
    }

    void resetRuntimeServices(const GameWorld& world) {
        g_runtime.erase(&world);
    }

    void updateRuntimeServices(GameWorld& world, const float dt) {
        auto& state = stateFor(world);
        for (auto& effect : state.effects) {
            effect.age += dt;
        }
        std::erase_if(state.effects, [](const ActiveEffect& effect) {
            return effect.age >= effect.duration;
        });
        if (state.feedback.size() > 256) {
            state.feedback.erase(state.feedback.begin(), state.feedback.end() - 256);
        }
    }

    void emitSound(GameWorld& world, const SoundCue cue,
                   const model::Vector2D position, const float volume) {
        auto& state = stateFor(world);
        state.feedback.push_back(FeedbackEvent {
            .serial = state.nextSerial++,
            .cue = cue,
            .position = position,
            .volume = volume
        });
    }

    void emitEffect(GameWorld& world, const EffectType type,
                    const model::Vector2D position, const float radius,
                    const float duration, const std::uint32_t color,
                    const float rotation) {
        auto& state = stateFor(world);
        // Effects are pooled by reusing the same active vector allocation; the cap
        // prevents visual spam from growing memory during large fights.
        if (state.effects.size() >= 192) {
            state.effects.erase(state.effects.begin());
        }
        state.effects.push_back(ActiveEffect {
            .type = type,
            .position = position,
            .radius = radius,
            .age = 0.0f,
            .duration = duration,
            .rotation = rotation,
            .color = color
        });
    }

    const std::vector<FeedbackEvent>& feedbackEvents(const GameWorld& world) {
        const auto it = g_runtime.find(&world);
        return it == g_runtime.end() ? g_emptyFeedback : it->second.feedback;
    }

    const std::vector<ActiveEffect>& activeEffects(const GameWorld& world) {
        const auto it = g_runtime.find(&world);
        return it == g_runtime.end() ? g_emptyEffects : it->second.effects;
    }

    void rebuildSpatialIndex(const GameWorld& world) {
        auto& state = stateFor(world);
        state.cellSize = std::max(128.0f, world.gridTransform().tileSize * 4.0f);
        state.spatial.clear();

        for (const auto& element : world.getElements()) {
            auto gameElement = std::dynamic_pointer_cast<model::IGameElement>(element);
            if (!gameElement || gameElement->getAction() == model::ActionType::Dead) {
                continue;
            }
            state.spatial[cellFor(state, gameElement->getPosition())].push_back(gameElement);
        }
    }

    std::vector<std::shared_ptr<model::IGameElement>> queryRadius(
        const GameWorld& world, const model::Vector2D center, const float radius) {
        auto& state = stateFor(world);
        if (state.spatial.empty()) {
            rebuildSpatialIndex(world);
        }

        std::vector<std::shared_ptr<model::IGameElement>> result;
        std::unordered_set<const void*> seen;
        const float radiusSq = radius * radius;
        const SpatialCell c = cellFor(state, center);
        const int range = std::max(1, static_cast<int>(std::ceil(radius / state.cellSize)));

        for (int y = c.y - range; y <= c.y + range; ++y) {
            for (int x = c.x - range; x <= c.x + range; ++x) {
                const auto it = state.spatial.find(SpatialCell{ x, y });
                if (it == state.spatial.end()) {
                    continue;
                }
                for (const auto& weak : it->second) {
                    auto element = weak.lock();
                    if (!element || element->getAction() == model::ActionType::Dead ||
                        !seen.insert(element.get()).second) {
                        continue;
                    }
                    if (distanceSq(center, element->getPosition()) <= radiusSq) {
                        result.push_back(std::move(element));
                    }
                }
            }
        }

        std::stable_sort(result.begin(), result.end(), [](const auto& lhs, const auto& rhs) {
            return lhs->entityId().index < rhs->entityId().index;
        });
        return result;
    }

    const char* soundCueKey(const SoundCue cue) {
        switch (cue) {
            case SoundCue::Select: return "select";
            case SoundCue::MoveOrder: return "move";
            case SoundCue::AttackOrder: return "attack";
            case SoundCue::AttackFire: return "fire";
            case SoundCue::Hit: return "hit";
            case SoundCue::Death: return "death";
            case SoundCue::ProductionComplete: return "production";
            case SoundCue::ConstructionComplete: return "construction";
            case SoundCue::ResourceShortage: return "shortage";
            case SoundCue::ResourceGather: return "gather";
            case SoundCue::Victory: return "victory";
            case SoundCue::Defeat: return "defeat";
            case SoundCue::Explosion: return "explosion";
        }
        return "select";
    }
}
