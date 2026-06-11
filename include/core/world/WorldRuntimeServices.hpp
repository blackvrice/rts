#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "core/model/Vector2D.hpp"

namespace rts::core::model {
    class IGameElement;
}

namespace rts::core::world {
    class GameWorld;

    enum class SoundCue : std::uint8_t {
        Select,
        MoveOrder,
        AttackOrder,
        AttackFire,
        Hit,
        Death,
        ProductionComplete,
        ConstructionComplete,
        ResourceShortage,
        ResourceGather,
        Victory,
        Defeat,
        Explosion
    };

    enum class EffectType : std::uint8_t {
        AttackFlash,
        HitSpark,
        DeathBurst,
        Explosion,
        ConstructionDust,
        ResourceGather,
        ScorchDecal,
        BloodDecal
    };

    struct FeedbackEvent {
        std::uint64_t serial { 0 };
        SoundCue cue { SoundCue::Select };
        model::Vector2D position {};
        float volume { 70.0f };
    };

    struct ActiveEffect {
        EffectType type { EffectType::HitSpark };
        model::Vector2D position {};
        float radius { 32.0f };
        float age { 0.0f };
        float duration { 0.45f };
        float rotation { 0.0f };
        std::uint32_t color { 0xFFFFFFFFu };
    };

    void resetRuntimeServices(const GameWorld& world);
    void updateRuntimeServices(GameWorld& world, float dt);

    void emitSound(GameWorld& world, SoundCue cue,
                   model::Vector2D position = {}, float volume = 70.0f);
    void emitEffect(GameWorld& world, EffectType type,
                    model::Vector2D position, float radius,
                    float duration, std::uint32_t color = 0xFFFFFFFFu,
                    float rotation = 0.0f);

    const std::vector<FeedbackEvent>& feedbackEvents(const GameWorld& world);
    const std::vector<ActiveEffect>& activeEffects(const GameWorld& world);

    void rebuildSpatialIndex(const GameWorld& world);
    std::vector<std::shared_ptr<model::IGameElement>> queryRadius(
        const GameWorld& world, model::Vector2D center, float radius);

    const char* soundCueKey(SoundCue cue);
}
