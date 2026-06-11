#pragma once

#include <vector>

#include "core/model/Building.hpp"
#include "core/model/UnitType.hpp"

namespace rts::core::data {
    // Tech upgrades that can be researched. There is no upgrade content yet, so the
    // enum only carries the None sentinel; the field/validator plumbing exists so
    // research can be added later without touching call sites. None never appears in
    // a requirement list nor in a researched set (it means "not a real upgrade").
    enum class UpgradeType {
        None
    };

    // A gate that must be satisfied before a unit/building/upgrade is available:
    // every listed building must be completed and every listed upgrade researched.
    struct Requirement {
        std::vector<::rts::core::model::BuildingType> requiredBuildings {};
        std::vector<UpgradeType> requiredUpgrades {};

        bool empty() const noexcept {
            return requiredBuildings.empty() && requiredUpgrades.empty();
        }
    };
}
