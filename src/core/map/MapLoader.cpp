#include "core/map/MapData.hpp"

#include <fstream>
#include <iostream>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

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
