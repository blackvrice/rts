#pragma once

#include <string>

#include "core/model/PlayerResourceState.hpp"
#include "core/model/UnitType.hpp"

namespace rts::core::data {
    // Damage class of a unit's attack. Pairs with ArmorType for a future
    // type-vs-type damage table; currently stored as design data.
    enum class WeaponType { Normal, Pierce, Siege, Magic };
    // Defensive class of a unit. See WeaponType.
    enum class ArmorType { Unarmored, Light, Heavy, Fortified };

    struct UnitStaticData {
        ::rts::UnitType unitType { ::rts::UnitType::Warrior };
        std::string displayName { "Unit" };
        float maxHp { 100.0f };
        float attackDamage { 10.0f };
        float attackRange { 80.0f };
        float attackCooldown { 0.8f };
        float moveSpeed { 120.0f };
        float armor { 0.0f };
        float sightRange { 256.0f };
        float collisionRadius { 28.0f };
        float buildTimeSeconds { 8.0f };
        WeaponType weaponType { WeaponType::Normal };
        ArmorType armorType { ArmorType::Light };
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
            .maxHp = 120.0f,
            .attackDamage = 12.0f,
            .attackRange = 80.0f,
            .attackCooldown = 0.75f,
            .moveSpeed = 120.0f,
            .armor = 1.0f,
            .sightRange = 256.0f,
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
            .maxHp = 80.0f,
            .attackDamage = 9.0f,
            .attackRange = 180.0f,
            .attackCooldown = 0.9f,
            .moveSpeed = 115.0f,
            .armor = 0.0f,
            .sightRange = 288.0f,
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
            .maxHp = 70.0f,
            .attackDamage = 4.0f,
            .attackRange = 48.0f,
            .attackCooldown = 1.0f,
            .moveSpeed = 110.0f,
            .armor = 0.0f,
            .sightRange = 224.0f,
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
        data.attackDamage = 8.0f;
        data.attackRange = 150.0f;
        data.buildTimeSeconds = 8.0f;
        data.goldCost = 50;
        data.woodCost = 0;
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
