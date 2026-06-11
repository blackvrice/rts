#pragma once

#include <set>

#include "core/data/TechTree.hpp"
#include "core/model/Building.hpp"
#include "core/model/UnitType.hpp"

namespace rts::core::tech {
    // Why an action is unavailable (None == available).
    enum class LockReason {
        None,
        MissingBuilding,    // a prerequisite building is absent or still under construction
        MissingUpgrade,     // a prerequisite upgrade has not been researched
        CannotProduceHere,  // no owned building can train this unit
        UnknownUpgrade      // upgrade has no definition yet (no research content)
    };

    struct TechResult {
        bool ok { false };
        LockReason reason { LockReason::None };
        // The first unmet building prerequisite (valid when reason == MissingBuilding).
        ::rts::core::model::BuildingType missingBuilding {};

        explicit operator bool() const noexcept { return ok; }
        static TechResult allow() noexcept { return { true, LockReason::None, {} }; }
    };

    // Snapshot of a single team's tech progress, built from the world by the caller:
    // which building types are completed and which upgrades are researched.
    struct TechState {
        std::set<::rts::core::model::BuildingType> completedBuildings;
        std::set<::rts::core::data::UpgradeType> researchedUpgrades;

        bool hasBuilding(::rts::core::model::BuildingType b) const {
            return completedBuildings.find(b) != completedBuildings.end();
        }
        bool hasUpgrade(::rts::core::data::UpgradeType u) const {
            return researchedUpgrades.find(u) != researchedUpgrades.end();
        }
    };

    // Stateless prerequisite checks against the DataRegistry's static requirements.
    class TechTreeValidator {
    public:
        // Can the team construct this building type (building + upgrade prereqs met)?
        static TechResult canBuild(const TechState& state,
                                   ::rts::core::model::BuildingType type);
        // Can the team train this unit? The producing building is enforced by the
        // caller's selection; this checks the unit's own tech requirement.
        static TechResult canProduce(const TechState& state, ::rts::UnitType type);
        // Can the team research this upgrade? No upgrade content exists yet, so this
        // reports UnknownUpgrade until upgrades are defined.
        static TechResult canResearch(const TechState& state,
                                      ::rts::core::data::UpgradeType type);

    private:
        static TechResult checkRequirement(const TechState& state,
                                           const ::rts::core::data::Requirement& req);
    };
}
