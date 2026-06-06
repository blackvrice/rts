#pragma once

#include <string>

#include "core/model/UnitType.hpp"

namespace rts::core::data {
    struct UnitStaticData {
        std::string displayName { "Unit" };
        float maxHp { 100.0f };
        float attackDamage { 10.0f };
        float attackRange { 80.0f };
        float attackCooldown { 0.8f };
        float moveSpeed { 120.0f };
        float armor { 0.0f };
        int goldCost { 0 };
        int woodCost { 0 };
        int foodCost { 0 };
    };

    inline UnitStaticData warriorUnitStaticData() {
        return UnitStaticData {
            .displayName = "Warrior",
            .maxHp = 120.0f,
            .attackDamage = 12.0f,
            .attackRange = 80.0f,
            .attackCooldown = 0.75f,
            .moveSpeed = 120.0f,
            .armor = 1.0f,
            .goldCost = 75,
            .woodCost = 0,
            .foodCost = 1
        };
    }

    inline UnitStaticData archerUnitStaticData() {
        return UnitStaticData {
            .displayName = "Archer",
            .maxHp = 80.0f,
            .attackDamage = 9.0f,
            .attackRange = 180.0f,
            .attackCooldown = 0.9f,
            .moveSpeed = 115.0f,
            .armor = 0.0f,
            .goldCost = 60,
            .woodCost = 35,
            .foodCost = 1
        };
    }

    inline UnitStaticData workerUnitStaticData() {
        return UnitStaticData {
            .displayName = "Worker",
            .maxHp = 70.0f,
            .attackDamage = 4.0f,
            .attackRange = 48.0f,
            .attackCooldown = 1.0f,
            .moveSpeed = 110.0f,
            .armor = 0.0f,
            .goldCost = 50,
            .woodCost = 0,
            .foodCost = 1
        };
    }

    inline UnitStaticData marineUnitStaticData() {
        auto data = archerUnitStaticData();
        data.displayName = "Marine";
        data.attackDamage = 8.0f;
        data.attackRange = 150.0f;
        data.goldCost = 50;
        data.woodCost = 0;
        return data;
    }

    inline UnitStaticData unitStaticDataFor(const ::rts::UnitType type) {
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
}

namespace rts::data {
    using ::rts::core::data::UnitStaticData;
}

namespace rts::unit {
    using ::rts::core::data::UnitStaticData;
}
