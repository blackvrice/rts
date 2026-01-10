//
// Created by black on 26. 1. 4..
//

#pragma once
#include <cstdint>

#include "Vector2D.hpp"

namespace rts::core::model {
    using UnitId  = uint32_t;
    using BuildId = uint32_t;

    constexpr UnitId  InvalidUnitId  = 0;
    constexpr BuildId InvalidBuildId = 0;


    enum class UnitAction : uint8_t {
        None = 0,
        Move,
        Attack,
        Hold,
        Build
    };

    struct GameState {
        Vector2D pos {};        // World position
        int      maxHp { 0 };
        int      hp    { 0 };
        bool     selected { false };

        bool alive() const {
            return hp > 0;
        }

        bool selectedUnits() const {
            return selected && maxHp > 0;
        }
    };


    // -----------------------------
    // Unit State (Logic Snapshot)
    // -----------------------------
    struct UnitState final : public GameState {
        UnitId     id { InvalidUnitId };
        UnitAction action { UnitAction::None };

        Vector2D   targetPos {};
        UnitId     targetId { InvalidUnitId };
    };
    // -----------------------------
    // Build State (Construction)
    // -----------------------------
    struct BuildState final : public GameState {
        BuildId id { InvalidBuildId };

        float progress { 0.f };
        float buildTime { 0.f };
    };
}
