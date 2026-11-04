#include "core/map/MapData.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>
#include <tmxlite/Map.hpp>
#include <tmxlite/Layer.hpp>
#include <tmxlite/TileLayer.hpp>
#include <tmxlite/ObjectGroup.hpp>
#include <tmxlite/Object.hpp>
#include <tmxlite/Property.hpp>

#include "core/data/DataRegistry.hpp"
#include "core/model/IGameElement.hpp"

namespace rts::core::map {
    namespace model = ::rts::core::model;
    using json = nlohmann::json;

    namespace {
        // Owner id from a map string: "player" = 1, "enemy" = 2, "pN"/"playerN"
        // or a bare number = N, anything else (e.g. "neutral") = 0.
        int teamFromString(const std::string& s) {
            if (s == "player") return model::TeamId::Player;
            if (s == "enemy")  return model::TeamId::Enemy;
            std::string digits = s;
            if (digits.rfind("player", 0) == 0) {
                digits = digits.substr(6);
            } else if (!digits.empty() && (digits[0] == 'p' || digits[0] == 'P')) {
                digits = digits.substr(1);
            }
            if (!digits.empty() &&
                std::all_of(digits.begin(), digits.end(),
                            [](unsigned char c) { return std::isdigit(c) != 0; })) {
                return std::stoi(digits);
            }
            return model::TeamId::Neutral;
        }

        // JSON "team" may be a string ("player", "p3") or an integer id.
        int teamFromJson(const json& e) {
            if (!e.contains("team")) return model::TeamId::Neutral;
            const auto& t = e["team"];
            if (t.is_number_integer()) return t.get<int>();
            if (t.is_string()) return teamFromString(t.get<std::string>());
            return model::TeamId::Neutral;
        }

        model::Vector2D readPos(const json& e) {
            return model::Vector2D {
                e.value("x", 0.0f),
                e.value("y", 0.0f)
            };
        }

        std::string toLower(std::string s) {
            std::transform(s.begin(), s.end(), s.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return s;
        }

        // Tiled property lookup helpers (string / int with float fallback).
        std::string tmxPropString(const std::vector<tmx::Property>& props,
                                   const std::string& name, const std::string& def = {}) {
            for (const auto& p : props) {
                if (p.getName() == name && p.getType() == tmx::Property::Type::String) {
                    return p.getStringValue();
                }
            }
            return def;
        }

        int tmxPropInt(const std::vector<tmx::Property>& props,
                       const std::string& name, const int def) {
            for (const auto& p : props) {
                if (p.getName() != name) continue;
                if (p.getType() == tmx::Property::Type::Int)   return p.getIntValue();
                if (p.getType() == tmx::Property::Type::Float) return static_cast<int>(p.getFloatValue());
            }
            return def;
        }
    }

    // Loads a Tiled .tmx map via tmxlite. Object layers carry the entities
    // (class = building/unit/resource, with "kind"/"team" properties); any tile
    // layer whose name contains "collis" marks blocked tiles. Map-level int
    // properties playerGold/playerWood/enemyGold/enemyWood seed the economy.
    MapData loadTmxMap(const std::string& path) {
        tmx::Map map;
        if (!map.load(path)) {
            std::cerr << "[MapLoader] cannot load TMX " << path
                      << " (using built-in default map)\n";
            return defaultMapData();
        }

        auto& registry = data::DataRegistry::global();
        MapData m;
        const auto tileCount = map.getTileCount();
        const auto tileSize = map.getTileSize();
        m.width = static_cast<int>(tileCount.x);
        m.height = static_cast<int>(tileCount.y);
        if (tileSize.x > 0) m.tileSize = static_cast<float>(tileSize.x);

        const auto& mapProps = map.getProperties();
        // playerGold/playerWood (id 1), enemyGold/enemyWood (id 2), pNGold/pNWood.
        const auto readTmxStart = [&](const int id, const std::string& prefix) {
            const MapData::StartResources defaults {};
            const int gold = tmxPropInt(mapProps, prefix + "Gold", -1);
            const int wood = tmxPropInt(mapProps, prefix + "Wood", -1);
            if (gold >= 0 || wood >= 0) {
                m.startResources[id] = MapData::StartResources {
                    gold >= 0 ? gold : defaults.gold,
                    wood >= 0 ? wood : defaults.wood
                };
            }
        };
        readTmxStart(model::TeamId::Player, "player");
        readTmxStart(model::TeamId::Enemy, "enemy");
        for (int id = 1; id <= 8; ++id) {
            readTmxStart(id, "p" + std::to_string(id));
        }

        for (const auto& layer : map.getLayers()) {
            if (layer->getType() == tmx::Layer::Type::Tile) {
                // Only a dedicated collision layer blocks; visual layers are ignored.
                if (toLower(layer->getName()).find("collis") == std::string::npos) {
                    continue;
                }
                const auto& tiles = layer->getLayerAs<tmx::TileLayer>().getTiles();
                for (std::size_t i = 0; i < tiles.size() && m.width > 0; ++i) {
                    if (tiles[i].ID != 0) {
                        m.blockedTiles.push_back(path::GridPos{
                            static_cast<int>(i % static_cast<std::size_t>(m.width)),
                            static_cast<int>(i / static_cast<std::size_t>(m.width)) });
                    }
                }
            } else if (layer->getType() == tmx::Layer::Type::Object) {
                for (const auto& obj : layer->getLayerAs<tmx::ObjectGroup>().getObjects()) {
                    const std::string cls = toLower(obj.getClass());
                    const auto& props = obj.getProperties();
                    const std::string kind = tmxPropString(props, "kind", obj.getName());
                    const std::string team = tmxPropString(props, "team", "neutral");
                    const model::Vector2D pos { obj.getPosition().x, obj.getPosition().y };
                    if (cls == "building") {
                        if (const auto* d = registry.buildingById(kind)) {
                            m.buildings.push_back({ d->buildingType, teamFromString(team), pos });
                        } else {
                            std::cerr << "[MapLoader] TMX unknown building kind '" << kind << "'\n";
                        }
                    } else if (cls == "unit") {
                        if (const auto* d = registry.unitById(kind)) {
                            m.units.push_back({ d->unitType, teamFromString(team), pos });
                        } else {
                            std::cerr << "[MapLoader] TMX unknown unit kind '" << kind << "'\n";
                        }
                    } else if (cls == "resource") {
                        if (const auto* d = registry.resourceById(kind)) {
                            m.resources.push_back({ d->resourceType, pos });
                        } else {
                            std::cerr << "[MapLoader] TMX unknown resource kind '" << kind << "'\n";
                        }
                    }
                    // Other classes (e.g. "start") are ignored for now.
                }
            }
        }

        std::cout << "[MapLoader] loaded TMX " << path << " (" << m.width << "x" << m.height
                  << ", " << m.buildings.size() << " buildings, " << m.units.size()
                  << " units, " << m.resources.size() << " resources)" << std::endl;
        return m;
    }

    MapData defaultMapData() {
        MapData m;
        m.width = 32;
        m.height = 32;
        m.tileSize = 64.0f;

        m.buildings = {
            { model::BuildingType::TownHall, model::TeamId::Player, { 220.f, 220.f } },
            { model::BuildingType::Barracks, model::TeamId::Player, { 500.f, 220.f } },
            { model::BuildingType::TownHall, model::TeamId::Enemy,  { 1200.f, 760.f } },
            { model::BuildingType::Barracks, model::TeamId::Enemy,  { 1480.f, 760.f } },
        };

        // One of each unit per team in a spaced row (110px columns).
        const ::rts::UnitType row[] = {
            ::rts::UnitType::Worker, ::rts::UnitType::Warrior,
            ::rts::UnitType::Archer, ::rts::UnitType::Marine
        };
        float x = 240.f;
        for (const auto type : row) { m.units.push_back({ type, model::TeamId::Player, { x, 460.f } }); x += 110.f; }
        x = 1200.f;
        for (const auto type : row) { m.units.push_back({ type, model::TeamId::Enemy, { x, 1000.f } }); x += 110.f; }

        m.resources = {
            { model::ResourceNode::ResourceType::Gold, { 120.f, 620.f } },
            { model::ResourceNode::ResourceType::Wood, { 120.f, 760.f } },
        };
        return m;
    }

    MapData loadMap(const std::string& path) {
        // Tiled .tmx maps go through tmxlite; everything else is the native JSON
        // scenario format.
        if (path.size() >= 4 && toLower(path.substr(path.size() - 4)) == ".tmx") {
            return loadTmxMap(path);
        }

        std::ifstream in(path);
        if (!in) {
            std::cerr << "[MapLoader] cannot open " << path
                      << " (using built-in default map)\n";
            return defaultMapData();
        }

        json doc;
        try {
            in >> doc;
        } catch (const std::exception& e) {
            std::cerr << "[MapLoader] parse error in " << path << ": " << e.what()
                      << " (using built-in default map)\n";
            return defaultMapData();
        }

        auto& registry = data::DataRegistry::global();
        MapData m;
        m.width = doc.value("width", m.width);
        m.height = doc.value("height", m.height);
        m.tileSize = doc.value("tileSize", m.tileSize);

        // Keys are owner names/ids ("player", "enemy", "p3", "4").
        if (doc.contains("startResources") && doc["startResources"].is_object()) {
            for (const auto& [key, value] : doc["startResources"].items()) {
                const int id = teamFromString(key);
                if (id == model::TeamId::Neutral || !value.is_object()) continue;
                const MapData::StartResources defaults {};
                m.startResources[id] = MapData::StartResources {
                    value.value("gold", defaults.gold),
                    value.value("wood", defaults.wood)
                };
            }
        }

        for (const auto& e : doc.value("buildings", json::array())) {
            const auto* data = registry.buildingById(e.value("type", std::string{}));
            if (!data) {
                std::cerr << "[MapLoader] unknown building '" << e.value("type", std::string{}) << "'\n";
                continue;
            }
            m.buildings.push_back({ data->buildingType, teamFromJson(e), readPos(e) });
        }

        for (const auto& e : doc.value("units", json::array())) {
            const auto* data = registry.unitById(e.value("type", std::string{}));
            if (!data) {
                std::cerr << "[MapLoader] unknown unit '" << e.value("type", std::string{}) << "'\n";
                continue;
            }
            m.units.push_back({ data->unitType, teamFromJson(e), readPos(e) });
        }

        for (const auto& e : doc.value("resources", json::array())) {
            const auto* data = registry.resourceById(e.value("type", std::string{}));
            if (!data) {
                std::cerr << "[MapLoader] unknown resource '" << e.value("type", std::string{}) << "'\n";
                continue;
            }
            m.resources.push_back({ data->resourceType, readPos(e) });
        }

        for (const auto& e : doc.value("blockedTiles", json::array())) {
            // Each entry is a [x, y] tile coordinate pair.
            if (e.is_array() && e.size() == 2) {
                m.blockedTiles.push_back(path::GridPos{ e[0].get<int>(), e[1].get<int>() });
            }
        }

        std::cout << "[MapLoader] loaded map " << path << " (" << m.width << "x" << m.height
                  << ", " << m.buildings.size() << " buildings, " << m.units.size()
                  << " units, " << m.resources.size() << " resources)" << std::endl;
        return m;
    }

    MapData::StartResources MapData::startResourcesFor(const int playerId) const {
        const auto it = startResources.find(playerId);
        return it != startResources.end() ? it->second : StartResources {};
    }

    std::vector<int> MapData::playerIds() const {
        std::vector<int> ids { model::TeamId::Player, model::TeamId::Enemy };
        const auto add = [&ids](const int id) {
            if (id != model::TeamId::Neutral &&
                std::find(ids.begin(), ids.end(), id) == ids.end()) {
                ids.push_back(id);
            }
        };
        for (const auto& [id, start] : startResources) add(id);
        for (const auto& b : buildings) add(b.teamId);
        for (const auto& u : units) add(u.teamId);
        std::sort(ids.begin(), ids.end());
        return ids;
    }
}
