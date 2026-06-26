#pragma once

#include <string>

#include "core/data/CombatTypes.hpp"
#include "core/data/TechTree.hpp"
#include "core/model/PlayerResourceState.hpp"
#include "core/model/UnitType.hpp"

namespace rts::core::data {
    struct UnitStaticData {
        ::rts::UnitType unitType { ::rts::UnitType::Warrior };
        std::string displayName { "Unit" };
        // HUD selection portrait (Tiny Swords-relative path); empty = generic fallback.
        std::string portrait {};
        float maxHp { 100.0f };
        float attackDamage { 10.0f };
        float attackRange { 80.0f };
        float attackCooldown { 0.8f };
        float moveSpeed { 120.0f };
        float armor { 0.0f };
        // Reveal radius authored in map tiles. World-space size is this value
        // multiplied by the current map tileSize.
        float sightRange { 8.0f };
        float collisionRadius { 28.0f };
        float buildTimeSeconds { 8.0f };
        WeaponType weaponType { WeaponType::Normal };
        ArmorType armorType { ArmorType::Light };
        SplashRadii splash {};  // ranged attacks splash when outer > 0
        MovementDomain domain { MovementDomain::Ground };  // layer this unit occupies
        bool attacksGround { true };   // can target Ground-domain enemies
        bool attacksAir { true };      // can target Air-domain enemies
        Requirement requirement {};    // tech gate to produce this unit
        int goldCost { 0 };
        int woodCost { 0 };
        int foodCost { 0 };

        ::rts::core::model::Cost cost() const {
            return { goldCost, woodCost, foodCost };
        }
    };

    inline UnitStaticData warriorUnitStaticData() {
        return UnitStaticData {
            .unitType = ::rts::UnitType::Warrior,
            .displayName = "Warrior",
            .portrait = "UI Elements/UI Elements/Human Avatars/Avatars_02.png",
            .maxHp = 120.0f,
            .attackDamage = 12.0f,
            .attackRange = 80.0f,
            .attackCooldown = 0.75f,
            .moveSpeed = 120.0f,
            .armor = 1.0f,
            .sightRange = 8.0f,
            .collisionRadius = 28.0f,
            .buildTimeSeconds = 8.0f,
            .weaponType = WeaponType::Normal,
            .armorType = ArmorType::Heavy,
            .goldCost = 75,
            .woodCost = 0,
            .foodCost = 1
        };
    }

    inline UnitStaticData archerUnitStaticData() {
        return UnitStaticData {
            .unitType = ::rts::UnitType::Archer,
            .displayName = "Archer",
            .portrait = "UI Elements/UI Elements/Human Avatars/Avatars_03.png",
            .maxHp = 80.0f,
            .attackDamage = 9.0f,
            .attackRange = 180.0f,
            .attackCooldown = 0.9f,
            .moveSpeed = 115.0f,
            .armor = 0.0f,
            .sightRange = 9.0f,
            .collisionRadius = 26.0f,
            .buildTimeSeconds = 9.0f,
            .weaponType = WeaponType::Pierce,
            .armorType = ArmorType::Light,
            .goldCost = 60,
            .woodCost = 35,
            .foodCost = 1
        };
    }

    inline UnitStaticData workerUnitStaticData() {
        return UnitStaticData {
            .unitType = ::rts::UnitType::Worker,
            .displayName = "Worker",
            .portrait = "UI Elements/UI Elements/Human Avatars/Avatars_01.png",
            .maxHp = 70.0f,
            .attackDamage = 4.0f,
            .attackRange = 48.0f,
            .attackCooldown = 1.0f,
            .moveSpeed = 110.0f,
            .armor = 0.0f,
            .sightRange = 7.0f,
            .collisionRadius = 26.0f,
            .buildTimeSeconds = 12.0f,
            .weaponType = WeaponType::Normal,
            .armorType = ArmorType::Light,
            .goldCost = 50,
            .woodCost = 0,
            .foodCost = 1
        };
    }

    inline UnitStaticData marineUnitStaticData() {
        auto data = archerUnitStaticData();
        data.unitType = ::rts::UnitType::Marine;
        data.displayName = "Marine";
        data.portrait = "UI Elements/UI Elements/Human Avatars/Avatars_04.png";
        data.attackDamage = 8.0f;
        data.attackRange = 150.0f;
        data.buildTimeSeconds = 8.0f;
        data.goldCost = 50;
        data.woodCost = 0;
        data.splash = { 24.0f, 40.0f, 56.0f };  // marine shots splash on impact
        return data;
    }

    // Built-in fallback table used to seed the DataRegistry and as a safety net
    // when no JSON override exists for a type.
    inline UnitStaticData defaultUnitStaticDataFor(const ::rts::UnitType type) {
        switch (type) {
            case ::rts::UnitType::Marine:
                return marineUnitStaticData();
            case ::rts::UnitType::Warrior:
                return warriorUnitStaticData();
            case ::rts::UnitType::Archer:
                return archerUnitStaticData();
            case ::rts::UnitType::Worker:
                return workerUnitStaticData();
        }

        return warriorUnitStaticData();
    }

    // Registry-backed lookup (data/units.json overrides the defaults). Defined
    // in DataRegistry.cpp.
    UnitStaticData unitStaticDataFor(::rts::UnitType type);
}

namespace rts::data {
    using ::rts::core::data::UnitStaticData;
}

namespace rts::unit {
    using ::rts::core::data::UnitStaticData;
}
