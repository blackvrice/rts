#include "core/data/DataRegistry.hpp"

#include <fstream>
#include <iostream>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace rts::core::data {
    namespace model = ::rts::core::model;
    using json = nlohmann::json;

    namespace {
        // String id -> internal enum id. The enum is a code concept; JSON refers
        // to it by a stable string. Unknown ids return nullopt (logged by caller).
        std::optional<::rts::UnitType> unitTypeFromId(const std::string& id) {
            if (id == "marine")  return ::rts::UnitType::Marine;
            if (id == "warrior") return ::rts::UnitType::Warrior;
            if (id == "archer")  return ::rts::UnitType::Archer;
            if (id == "worker")  return ::rts::UnitType::Worker;
            return std::nullopt;
        }

        std::optional<model::BuildingType> buildingTypeFromId(const std::string& id) {
            if (id == "town_hall") return model::BuildingType::TownHall;
            if (id == "barracks")  return model::BuildingType::Barracks;
            return std::nullopt;
        }

        std::optional<ResourceType> resourceTypeFromId(const std::string& id) {
            if (id == "gold") return ResourceType::Gold;
            if (id == "wood") return ResourceType::Wood;
            return std::nullopt;
        }

        WeaponType weaponTypeFromString(const std::string& s, const WeaponType fallback) {
            if (s == "normal") return WeaponType::Normal;
            if (s == "pierce") return WeaponType::Pierce;
            if (s == "siege")  return WeaponType::Siege;
            if (s == "magic")  return WeaponType::Magic;
            return fallback;
        }

        ArmorType armorTypeFromString(const std::string& s, const ArmorType fallback) {
            if (s == "unarmored") return ArmorType::Unarmored;
            if (s == "light")     return ArmorType::Light;
            if (s == "heavy")     return ArmorType::Heavy;
            if (s == "fortified") return ArmorType::Fortified;
            return fallback;
        }

        bool readJsonFile(const std::string& path, json& out) {
            std::ifstream in(path);
            if (!in) {
                std::cerr << "[DataRegistry] cannot open " << path
                          << " (keeping built-in defaults)\n";
                return false;
            }
            try {
                in >> out;
            } catch (const std::exception& e) {
                std::cerr << "[DataRegistry] parse error in " << path << ": "
                          << e.what() << " (keeping built-in defaults)\n";
                return false;
            }
            return true;
        }

        // Logs a warning and returns false when a numeric field is non-positive.
        bool requirePositive(const char* field, const std::string& id, double value) {
            if (value <= 0.0) {
                std::cerr << "[DataRegistry] '" << id << "': " << field
                          << " must be > 0 (was " << value << "), entry skipped\n";
                return false;
            }
            return true;
        }
    }

    DataRegistry::DataRegistry() {
        seedDefaults();
    }

    DataRegistry& DataRegistry::global() {
        static DataRegistry instance;
        return instance;
    }

    void DataRegistry::seedDefaults() {
        for (const auto type : { ::rts::UnitType::Marine, ::rts::UnitType::Warrior,
                                 ::rts::UnitType::Archer, ::rts::UnitType::Worker }) {
            m_units[type] = defaultUnitStaticDataFor(type);
        }
        for (const auto type : { model::BuildingType::TownHall, model::BuildingType::Barracks }) {
            m_buildings[type] = defaultBuildingStaticDataFor(type);
        }
        for (const auto type : { ResourceType::Gold, ResourceType::Wood }) {
            m_resources[type] = defaultResourceStaticDataFor(type);
        }

        m_unitIds = {
            { "marine", ::rts::UnitType::Marine },
            { "warrior", ::rts::UnitType::Warrior },
            { "archer", ::rts::UnitType::Archer },
            { "worker", ::rts::UnitType::Worker },
        };
        m_buildingIds = {
            { "town_hall", model::BuildingType::TownHall },
            { "barracks", model::BuildingType::Barracks },
        };
        m_resourceIds = {
            { "gold", ResourceType::Gold },
            { "wood", ResourceType::Wood },
        };
    }

    bool DataRegistry::loadFromDirectory(const std::string& dir) {
        const std::string base = dir.empty() || dir.back() == '/' || dir.back() == '\\'
            ? dir
            : dir + "/";

        bool allOk = true;
        int unitCount = 0, buildingCount = 0, resourceCount = 0;

        // ----- Units -----
        if (json doc; readJsonFile(base + "units.json", doc)) {
            for (const auto& e : doc.value("units", json::array())) {
                const auto id = e.value("id", std::string{});
                const auto type = unitTypeFromId(id);
                if (!type) {
                    std::cerr << "[DataRegistry] unknown unit id '" << id << "', skipped\n";
                    allOk = false;
                    continue;
                }
                // Merge over the seeded default so partial entries are valid.
                UnitStaticData d = m_units[*type];
                d.unitType = *type;
                d.displayName   = e.value("displayName", d.displayName);
                d.maxHp         = e.value("maxHp", d.maxHp);
                d.attackDamage  = e.value("attackDamage", d.attackDamage);
                d.attackRange   = e.value("attackRange", d.attackRange);
                d.attackCooldown= e.value("attackCooldown", d.attackCooldown);
                d.moveSpeed     = e.value("moveSpeed", d.moveSpeed);
                d.armor         = e.value("armor", d.armor);
                d.sightRange    = e.value("sightRange", d.sightRange);
                d.collisionRadius = e.value("collisionRadius", d.collisionRadius);
                d.buildTimeSeconds= e.value("buildTimeSeconds", d.buildTimeSeconds);
                d.weaponType    = weaponTypeFromString(e.value("weaponType", std::string{}), d.weaponType);
                d.armorType     = armorTypeFromString(e.value("armorType", std::string{}), d.armorType);
                d.goldCost      = e.value("goldCost", d.goldCost);
                d.woodCost      = e.value("woodCost", d.woodCost);
                d.foodCost      = e.value("foodCost", d.foodCost);
                if (!requirePositive("maxHp", id, d.maxHp)) { allOk = false; continue; }
                if (!requirePositive("collisionRadius", id, d.collisionRadius)) { allOk = false; continue; }
                m_units[*type] = d;
                ++unitCount;
            }
        } else {
            allOk = false;
        }

        // ----- Buildings -----
        if (json doc; readJsonFile(base + "buildings.json", doc)) {
            for (const auto& e : doc.value("buildings", json::array())) {
                const auto id = e.value("id", std::string{});
                const auto type = buildingTypeFromId(id);
                if (!type) {
                    std::cerr << "[DataRegistry] unknown building id '" << id << "', skipped\n";
                    allOk = false;
                    continue;
                }
                BuildingStaticData d = m_buildings[*type];
                d.buildingType    = *type;
                d.displayName     = e.value("displayName", d.displayName);
                d.maxHp           = e.value("maxHp", d.maxHp);
                d.footprintWidth  = e.value("footprintWidth", d.footprintWidth);
                d.footprintHeight = e.value("footprintHeight", d.footprintHeight);
                d.buildTimeSeconds= e.value("buildTimeSeconds", d.buildTimeSeconds);
                d.goldCost        = e.value("goldCost", d.goldCost);
                d.woodCost        = e.value("woodCost", d.woodCost);
                d.providesSupply  = e.value("providesSupply", d.providesSupply);
                d.isDropOff       = e.value("isDropOff", d.isDropOff);
                // produces: list of unit string ids -> UnitType (unknown ids skipped).
                if (e.contains("produces")) {
                    d.produces.clear();
                    for (const auto& uid : e["produces"]) {
                        const auto ut = unitTypeFromId(uid.get<std::string>());
                        if (ut) {
                            d.produces.push_back(*ut);
                        } else {
                            std::cerr << "[DataRegistry] building '" << id << "' produces unknown unit '"
                                      << uid << "', skipped\n";
                            allOk = false;
                        }
                    }
                }
                // requirements: list of building string ids -> BuildingType.
                if (e.contains("requirements")) {
                    d.requirements.clear();
                    for (const auto& bid : e["requirements"]) {
                        const auto bt = buildingTypeFromId(bid.get<std::string>());
                        if (bt) {
                            d.requirements.push_back(*bt);
                        } else {
                            std::cerr << "[DataRegistry] building '" << id << "' requires unknown building '"
                                      << bid << "', skipped\n";
                            allOk = false;
                        }
                    }
                }
                if (!requirePositive("maxHp", id, d.maxHp)) { allOk = false; continue; }
                if (!requirePositive("footprintWidth", id, d.footprintWidth)) { allOk = false; continue; }
                if (!requirePositive("footprintHeight", id, d.footprintHeight)) { allOk = false; continue; }
                m_buildings[*type] = d;
                ++buildingCount;
            }
        } else {
            allOk = false;
        }

        // ----- Resources -----
        if (json doc; readJsonFile(base + "resources.json", doc)) {
            for (const auto& e : doc.value("resources", json::array())) {
                const auto id = e.value("id", std::string{});
                const auto type = resourceTypeFromId(id);
                if (!type) {
                    std::cerr << "[DataRegistry] unknown resource id '" << id << "', skipped\n";
                    allOk = false;
                    continue;
                }
                ResourceStaticData d = m_resources[*type];
                d.resourceType         = *type;
                d.displayName          = e.value("displayName", d.displayName);
                d.initialAmount        = e.value("initialAmount", d.initialAmount);
                d.gatherAmountPerTrip  = e.value("gatherAmountPerTrip", d.gatherAmountPerTrip);
                d.gatherDurationSeconds= e.value("gatherDurationSeconds", d.gatherDurationSeconds);
                d.maxGatherers         = e.value("maxGatherers", d.maxGatherers);
                if (!requirePositive("initialAmount", id, d.initialAmount)) { allOk = false; continue; }
                m_resources[*type] = d;
                ++resourceCount;
            }
        } else {
            allOk = false;
        }

        std::cout << "[DataRegistry] loaded " << unitCount << " units, "
                  << buildingCount << " buildings, " << resourceCount
                  << " resources from " << dir
                  << (allOk ? "" : " (with warnings)") << std::endl;
        return allOk;
    }

    const UnitStaticData& DataRegistry::unit(const ::rts::UnitType type) const {
        if (const auto it = m_units.find(type); it != m_units.end()) {
            return it->second;
        }
        static const UnitStaticData fallback = defaultUnitStaticDataFor(::rts::UnitType::Warrior);
        return fallback;
    }

    const BuildingStaticData& DataRegistry::building(const model::BuildingType type) const {
        if (const auto it = m_buildings.find(type); it != m_buildings.end()) {
            return it->second;
        }
        static const BuildingStaticData fallback =
            defaultBuildingStaticDataFor(model::BuildingType::Barracks);
        return fallback;
    }

    const ResourceStaticData& DataRegistry::resource(const ResourceType type) const {
        if (const auto it = m_resources.find(type); it != m_resources.end()) {
            return it->second;
        }
        static const ResourceStaticData fallback = defaultResourceStaticDataFor(ResourceType::Gold);
        return fallback;
    }

    const UnitStaticData* DataRegistry::unitById(const std::string& id) const {
        const auto it = m_unitIds.find(id);
        return it == m_unitIds.end() ? nullptr : &unit(it->second);
    }

    const BuildingStaticData* DataRegistry::buildingById(const std::string& id) const {
        const auto it = m_buildingIds.find(id);
        return it == m_buildingIds.end() ? nullptr : &building(it->second);
    }

    const ResourceStaticData* DataRegistry::resourceById(const std::string& id) const {
        const auto it = m_resourceIds.find(id);
        return it == m_resourceIds.end() ? nullptr : &resource(it->second);
    }

    // ----- Registry-backed free functions declared in the *StaticData headers -----
    UnitStaticData unitStaticDataFor(const ::rts::UnitType type) {
        return DataRegistry::global().unit(type);
    }

    BuildingStaticData buildingStaticDataFor(const model::BuildingType type) {
        return DataRegistry::global().building(type);
    }

    ResourceStaticData resourceStaticDataFor(const ResourceType type) {
        return DataRegistry::global().resource(type);
    }
}
