#include "core/tech/TechTreeValidator.hpp"

#include "core/data/BuildingStaticData.hpp"
#include "core/data/DataRegistry.hpp"
#include "core/data/UnitStaticData.hpp"

namespace rts::core::tech {
    TechResult TechTreeValidator::checkRequirement(
        const TechState& state, const ::rts::core::data::Requirement& req) {
        for (const auto building : req.requiredBuildings) {
            if (!state.hasBuilding(building)) {
                return { false, LockReason::MissingBuilding, building };
            }
        }
        for (const auto upgrade : req.requiredUpgrades) {
            if (!state.hasUpgrade(upgrade)) {
                return { false, LockReason::MissingUpgrade, {} };
            }
        }
        return TechResult::allow();
    }

    TechResult TechTreeValidator::canBuild(
        const TechState& state, const ::rts::core::model::BuildingType type) {
        const auto& data = ::rts::core::data::DataRegistry::global().building(type);
        // BuildingStaticData stores building prereqs and upgrade prereqs separately;
        // fold them into a Requirement for the shared check.
        ::rts::core::data::Requirement req;
        req.requiredBuildings = data.requirements;
        req.requiredUpgrades = data.requiredUpgrades;
        return checkRequirement(state, req);
    }

    TechResult TechTreeValidator::canProduce(
        const TechState& state, const ::rts::UnitType type) {
        const auto& data = ::rts::core::data::DataRegistry::global().unit(type);
        return checkRequirement(state, data.requirement);
    }

    TechResult TechTreeValidator::canResearch(
        const TechState& state, const ::rts::core::data::UpgradeType type) {
        // No upgrades are defined yet; researching anything but a real upgrade fails.
        // When upgrade content is added, look up its Requirement here and delegate to
        // checkRequirement (and reject if already researched).
        (void)state;
        (void)type;
        return { false, LockReason::UnknownUpgrade, {} };
    }
}
