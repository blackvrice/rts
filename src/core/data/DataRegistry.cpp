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

        MovementDomain movementDomainFromString(const std::string& s, const MovementDomain fallback) {
            if (s == "ground") return MovementDomain::Ground;
            if (s == "air")    return MovementDomain::Air;
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

        seedSprites();
    }

    void DataRegistry::seedSprites() {
        // Built-in clips mirror the previously hard-coded Tiny Swords layout, so
        // rendering still works when data/animations.json is absent.
        auto unitClip = [](std::string texture, int frameCount, float fps) {
            return SpriteClip {
                .texture = std::move(texture), .frameCount = frameCount, .fps = fps,
                .sourceX = 0, .sourceY = 0, .sourceW = 192, .sourceH = 192,
                .displayW = 96.f, .displayH = 96.f, .anchorX = 48.f, .anchorY = 96.f,
                .trim = true
            };
        };
        m_sprites["unit.warrior.blue.idle"]   = unitClip("Units/Blue Units/Warrior/Warrior_Idle.png", 8, 6.f);
        m_sprites["unit.warrior.blue.move"]   = unitClip("Units/Blue Units/Warrior/Warrior_Run.png", 6, 10.f);
        m_sprites["unit.warrior.blue.attack"] = unitClip("Units/Blue Units/Warrior/Warrior_Attack1.png", 4, 8.f);
        m_sprites["unit.warrior.blue.hold"]   = unitClip("Units/Blue Units/Warrior/Warrior_Guard.png", 6, 6.f);
        m_sprites["unit.warrior.red.idle"]    = unitClip("Units/Red Units/Warrior/Warrior_Idle.png", 8, 6.f);
        m_sprites["unit.warrior.red.move"]    = unitClip("Units/Red Units/Warrior/Warrior_Run.png", 6, 10.f);
        m_sprites["unit.warrior.red.attack"]  = unitClip("Units/Red Units/Warrior/Warrior_Attack1.png", 4, 8.f);
        m_sprites["unit.warrior.red.hold"]    = unitClip("Units/Red Units/Warrior/Warrior_Guard.png", 6, 6.f);
        const auto seedPawn = [&](const std::string& teamKey, const std::string& teamFolder) {
            const std::string prefix = "unit.pawn." + teamKey + ".";
            const std::string folder = "Units/" + teamFolder + " Units/Pawn/";
            const auto add = [&](const std::string& key, const std::string& file,
                                 const int frames, const float fps) {
                m_sprites[prefix + key] = unitClip(folder + file, frames, fps);
            };

            add("idle", "Pawn_Idle.png", 8, 6.f);
            add("idle.axe", "Pawn_Idle Axe.png", 8, 6.f);
            add("idle.gold", "Pawn_Idle Gold.png", 8, 6.f);
            add("idle.hammer", "Pawn_Idle Hammer.png", 8, 6.f);
            add("idle.knife", "Pawn_Idle Knife.png", 8, 6.f);
            add("idle.meat", "Pawn_Idle Meat.png", 8, 6.f);
            add("idle.pickaxe", "Pawn_Idle Pickaxe.png", 8, 6.f);
            add("idle.wood", "Pawn_Idle Wood.png", 8, 6.f);
            add("move", "Pawn_Run.png", 6, 10.f);
            add("move.axe", "Pawn_Run Axe.png", 6, 10.f);
            add("move.gold", "Pawn_Run Gold.png", 6, 10.f);
            add("move.hammer", "Pawn_Run Hammer.png", 6, 10.f);
            add("move.knife", "Pawn_Run Knife.png", 6, 10.f);
            add("move.meat", "Pawn_Run Meat.png", 6, 10.f);
            add("move.pickaxe", "Pawn_Run Pickaxe.png", 6, 10.f);
            add("move.wood", "Pawn_Run Wood.png", 6, 10.f);
            add("attack", "Pawn_Interact Knife.png", 4, 8.f);
            add("attack.knife", "Pawn_Interact Knife.png", 4, 8.f);
            add("attack.hammer", "Pawn_Interact Hammer.png", 3, 8.f);
            add("build", "Pawn_Interact Hammer.png", 3, 8.f);
            add("gather.axe", "Pawn_Interact Axe.png", 6, 8.f);
            add("gather.pickaxe", "Pawn_Interact Pickaxe.png", 6, 8.f);
        };
        seedPawn("blue", "Blue");
        seedPawn("red", "Red");

        m_unitSpriteSets = {
            { "warrior", "warrior" }, { "archer", "warrior" },
            { "marine", "warrior" }, { "worker", "pawn" },
        };

        auto buildingClip = [](std::string texture) {
            return SpriteClip {
                .texture = std::move(texture), .frameCount = 1, .fps = 0.f,
                .sourceX = 0, .sourceY = 0, .sourceW = 0, .sourceH = 0,
                .displayW = 128.f, .displayH = 128.f, .anchorX = 64.f, .anchorY = 88.f,
                .trim = true
            };
        };
        m_sprites["building.town_hall.blue"] = buildingClip("Buildings/Blue Buildings/Castle.png");
        m_sprites["building.barracks.blue"]  = buildingClip("Buildings/Blue Buildings/Barracks.png");
        m_sprites["building.town_hall.red"]  = buildingClip("Buildings/Red Buildings/Castle.png");
        m_sprites["building.barracks.red"]   = buildingClip("Buildings/Red Buildings/Barracks.png");

        for (int stage = 1; stage <= 6; ++stage) {
            m_sprites["resource.gold." + std::to_string(stage)] = SpriteClip {
                .texture = "Terrain/Resources/Gold/Gold Stones/Gold Stone "
                    + std::to_string(stage) + "_Highlight.png",
                .frameCount = 6, .fps = 8.f,
                .sourceX = 0, .sourceY = 0, .sourceW = 128, .sourceH = 128,
                .displayW = 64.f, .displayH = 64.f, .anchorX = 32.f, .anchorY = 48.f,
                .trim = false
            };
        }
        m_sprites["resource.wood"] = SpriteClip {
            .texture = "Terrain/Resources/Wood/Wood Resource/Wood Resource.png",
            .frameCount = 1, .fps = 0.f,
            .sourceX = 0, .sourceY = 0, .sourceW = 0, .sourceH = 0,
            .displayW = 64.f, .displayH = 64.f, .anchorX = 32.f, .anchorY = 48.f,
            .trim = false
        };

        auto commandClip = [](std::string texture, const bool trim = false) {
            return SpriteClip {
                .texture = std::move(texture), .frameCount = 1, .fps = 0.f,
                .sourceX = 0, .sourceY = 0, .sourceW = 0, .sourceH = 0,
                .displayW = 30.f, .displayH = 30.f, .anchorX = 15.f, .anchorY = 15.f,
                .trim = trim
            };
        };
        m_sprites["command.default"] = commandClip("UI Elements/UI Elements/Icons/Icon_12.png");
        m_sprites["command.move"] = commandClip("UI Elements/UI Elements/Icons/Icon_07.png");
        m_sprites["command.stop"] = commandClip("UI Elements/UI Elements/Icons/Icon_09.png");
        m_sprites["command.hold"] = commandClip("UI Elements/UI Elements/Icons/Icon_05.png");
        m_sprites["command.gather"] = commandClip("UI Elements/UI Elements/Icons/Icon_01.png");
        m_sprites["command.build"] = commandClip("UI Elements/UI Elements/Icons/Icon_06.png");
        m_sprites["command.attack_move"] = commandClip("UI Elements/UI Elements/Icons/Icon_04.png");
        m_sprites["command.patrol"] = commandClip("UI Elements/UI Elements/Icons/Icon_08.png");
        m_sprites["command.cancel"] = commandClip("UI Elements/UI Elements/Icons/Icon_12.png");
        m_sprites["command.train.worker"] = commandClip("UI Elements/UI Elements/Human Avatars/Avatars_01.png");
        m_sprites["command.train.warrior"] = commandClip("UI Elements/UI Elements/Human Avatars/Avatars_02.png");
        m_sprites["command.train.archer"] = commandClip("UI Elements/UI Elements/Human Avatars/Avatars_03.png");
        m_sprites["command.train.marine"] = commandClip("UI Elements/UI Elements/Human Avatars/Avatars_04.png");
        m_sprites["command.build.town_hall"] = commandClip("Buildings/Blue Buildings/Castle.png", true);
        m_sprites["command.build.barracks"] = commandClip("Buildings/Blue Buildings/Barracks.png", true);
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
                d.portrait      = e.value("portrait", d.portrait);
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
                d.domain        = movementDomainFromString(e.value("domain", std::string{}), d.domain);
                d.attacksGround = e.value("attacksGround", d.attacksGround);
                d.attacksAir    = e.value("attacksAir", d.attacksAir);
                // requirements: building string ids that must be completed to train
                // this unit (unknown ids skipped). Upgrades have no ids defined yet.
                if (e.contains("requirements")) {
                    d.requirement.requiredBuildings.clear();
                    for (const auto& bid : e["requirements"]) {
                        const auto bt = buildingTypeFromId(bid.get<std::string>());
                        if (bt) {
                            d.requirement.requiredBuildings.push_back(*bt);
                        } else {
                            std::cerr << "[DataRegistry] unit '" << id << "' requires unknown building '"
                                      << bid << "', skipped\n";
                            allOk = false;
                        }
                    }
                }
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
                d.portrait        = e.value("portrait", d.portrait);
                d.maxHp           = e.value("maxHp", d.maxHp);
                d.footprintWidth  = e.value("footprintWidth", d.footprintWidth);
                d.footprintHeight = e.value("footprintHeight", d.footprintHeight);
                d.sightRange      = e.value("sightRange", d.sightRange);
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
                if (e.contains("sightRange") && !requirePositive("sightRange", id, d.sightRange)) {
                    allOk = false;
                    continue;
                }
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
                d.footprintWidth       = e.value("footprintWidth", d.footprintWidth);
                d.footprintHeight      = e.value("footprintHeight", d.footprintHeight);
                if (!requirePositive("initialAmount", id, d.initialAmount)) { allOk = false; continue; }
                if (!requirePositive("footprintWidth", id, d.footprintWidth)) { allOk = false; continue; }
                if (!requirePositive("footprintHeight", id, d.footprintHeight)) { allOk = false; continue; }
                m_resources[*type] = d;
                ++resourceCount;
            }
        } else {
            allOk = false;
        }

        // ----- Sprites / animations -----
        int spriteCount = 0;
        if (json doc; readJsonFile(base + "animations.json", doc)) {
            // A readable file is authoritative: drop the seeded clips so entries
            // removed from JSON are actually gone (add/remove/change via data).
            m_sprites.clear();
            m_unitSpriteSets.clear();
            // Bind to named objects: .items() on a temporary json would dangle.
            const json sets = doc.value("unitSpriteSets", json::object());
            for (const auto& [id, set] : sets.items()) {
                m_unitSpriteSets[id] = set.get<std::string>();
            }
            const json sprites = doc.value("sprites", json::object());
            for (const auto& [key, e] : sprites.items()) {
                // Merge over any seeded default for this key so partial overrides work.
                SpriteClip c = m_sprites.count(key) ? m_sprites[key] : SpriteClip{};
                c.texture     = e.value("texture", c.texture);
                c.frameCount  = e.value("frameCount", c.frameCount);
                c.fps         = e.value("fps", c.fps);
                c.sourceX     = e.value("sourceX", c.sourceX);
                c.sourceY     = e.value("sourceY", c.sourceY);
                c.sourceW     = e.value("sourceW", c.sourceW);
                c.sourceH     = e.value("sourceH", c.sourceH);
                c.displayW    = e.value("displayW", c.displayW);
                c.displayH    = e.value("displayH", c.displayH);
                c.anchorX     = e.value("anchorX", c.anchorX);
                c.anchorY     = e.value("anchorY", c.anchorY);
                c.trim        = e.value("trim", c.trim);
                if (c.texture.empty()) {
                    std::cerr << "[DataRegistry] sprite '" << key << "' has no texture, skipped\n";
                    allOk = false;
                    continue;
                }
                m_sprites[key] = std::move(c);
                ++spriteCount;
            }
        } else {
            allOk = false;
        }

        std::cout << "[DataRegistry] loaded " << unitCount << " units, "
                  << buildingCount << " buildings, " << resourceCount
                  << " resources, " << spriteCount << " sprites from " << dir
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

    const SpriteClip* DataRegistry::sprite(const std::string& key) const {
        const auto it = m_sprites.find(key);
        return it == m_sprites.end() ? nullptr : &it->second;
    }

    std::string DataRegistry::unitSpriteSet(const std::string& unitId) const {
        const auto it = m_unitSpriteSets.find(unitId);
        return it == m_unitSpriteSets.end() ? unitId : it->second;
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
