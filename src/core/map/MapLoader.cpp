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
        int teamFromString(const std::string& s) {
            if (s == "player") return model::TeamId::Player;
            if (s == "enemy")  return model::TeamId::Enemy;
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
        m.playerGold = tmxPropInt(mapProps, "playerGold", m.playerGold);
        m.playerWood = tmxPropInt(mapProps, "playerWood", m.playerWood);
        m.enemyGold  = tmxPropInt(mapProps, "enemyGold",  m.enemyGold);
        m.enemyWood  = tmxPropInt(mapProps, "enemyWood",  m.enemyWood);

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

        if (doc.contains("startResources")) {
            const auto& sr = doc["startResources"];
            if (sr.contains("player")) {
                m.playerGold = sr["player"].value("gold", m.playerGold);
                m.playerWood = sr["player"].value("wood", m.playerWood);
            }
            if (sr.contains("enemy")) {
                m.enemyGold = sr["enemy"].value("gold", m.enemyGold);
                m.enemyWood = sr["enemy"].value("wood", m.enemyWood);
            }
        }

        for (const auto& e : doc.value("buildings", json::array())) {
            const auto* data = registry.buildingById(e.value("type", std::string{}));
            if (!data) {
                std::cerr << "[MapLoader] unknown building '" << e.value("type", std::string{}) << "'\n";
                continue;
            }
            m.buildings.push_back({ data->buildingType, teamFromString(e.value("team", std::string{})), readPos(e) });
        }

        for (const auto& e : doc.value("units", json::array())) {
            const auto* data = registry.unitById(e.value("type", std::string{}));
            if (!data) {
                std::cerr << "[MapLoader] unknown unit '" << e.value("type", std::string{}) << "'\n";
                continue;
            }
            m.units.push_back({ data->unitType, teamFromString(e.value("team", std::string{})), readPos(e) });
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
}
