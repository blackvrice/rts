#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

#include "core/command/LogicCommand.hpp"
#include "core/data/DataPaths.hpp"
#include "core/data/DataRegistry.hpp"
#include "core/map/MapData.hpp"
#include "core/model/Building.hpp"
#include "core/model/ResourceNode.hpp"
#include "core/replay/ReplayLog.hpp"
#include "core/sim/Fixed.hpp"
#include "core/tech/TechTreeValidator.hpp"

namespace {
    int g_failures = 0;

    void expect(const bool condition, const char* message) {
        if (!condition) {
            std::cerr << "[rts_headless_smoke] FAIL: " << message << '\n';
            ++g_failures;
        }
    }

    std::string dataPath(const char* relative) {
        return (std::filesystem::path(rts::core::data::DataRoot) / relative).string();
    }

    void testDataRegistry() {
        auto& registry = rts::core::data::DataRegistry::global();
        expect(registry.loadFromDirectory(rts::core::data::DataRoot),
               "DataRegistry loads all runtime JSON files");

        const auto* worker = registry.unitById("worker");
        const auto* barracks = registry.buildingById("barracks");
        const auto* gold = registry.resourceById("gold");
        expect(worker != nullptr, "worker unit id resolves");
        expect(barracks != nullptr, "barracks building id resolves");
        expect(gold != nullptr, "gold resource id resolves");
        expect(registry.sprite("command.default") != nullptr, "command.default sprite resolves");

        if (worker) {
            expect(worker->maxHp > 0.0f, "worker maxHp is positive");
            expect(worker->sightRange > 0.0f, "worker sightRange is positive");
        }
        if (barracks) {
            expect(!barracks->produces.empty(), "barracks has trainable units");
            expect(!barracks->requirements.empty(), "barracks keeps Town Hall prerequisite");
        }
        if (gold) {
            expect(gold->initialAmount > 0, "gold initial amount is positive");
        }
    }

    void testMapLoading() {
        const auto jsonMap = rts::core::map::loadMap(dataPath("maps/skirmish.json"));
        expect(jsonMap.width == 256, "skirmish.json width matches authored map");
        expect(jsonMap.height == 256, "skirmish.json height matches authored map");
        expect(jsonMap.tileSize == 16.0f, "skirmish.json tile size matches authored map");
        expect(jsonMap.buildings.size() >= 4, "skirmish.json loads starting buildings");
        expect(jsonMap.units.size() >= 2, "skirmish.json loads starting units");
        expect(jsonMap.resources.size() >= 2, "skirmish.json loads resources");

        const auto tmxMap = rts::core::map::loadMap(dataPath("maps/tiled_skirmish.tmx"));
        expect(tmxMap.width > 0, "tiled_skirmish.tmx has positive width");
        expect(tmxMap.height > 0, "tiled_skirmish.tmx has positive height");
        expect(tmxMap.tileSize > 0.0f, "tiled_skirmish.tmx has positive tile size");
        expect(tmxMap.buildings.size() >= 2, "tiled_skirmish.tmx loads buildings");
        expect(tmxMap.units.size() >= 2, "tiled_skirmish.tmx loads units");
        expect(tmxMap.resources.size() >= 2, "tiled_skirmish.tmx loads resources");
    }

    void testTechTree() {
        using rts::core::model::BuildingType;
        using rts::core::tech::LockReason;
        using rts::core::tech::TechState;
        using rts::core::tech::TechTreeValidator;

        const TechState empty {};
        expect(TechTreeValidator::canBuild(empty, BuildingType::TownHall).ok,
               "Town Hall can be built without prerequisites");

        const auto barracksWithoutTownHall =
            TechTreeValidator::canBuild(empty, BuildingType::Barracks);
        expect(!barracksWithoutTownHall.ok, "Barracks is locked without Town Hall");
        expect(barracksWithoutTownHall.reason == LockReason::MissingBuilding,
               "Barracks lock reason is MissingBuilding");

        TechState withTownHall {};
        withTownHall.completedBuildings.insert(BuildingType::TownHall);
        expect(TechTreeValidator::canBuild(withTownHall, BuildingType::Barracks).ok,
               "Barracks unlocks after Town Hall");
        expect(TechTreeValidator::canProduce(withTownHall, rts::UnitType::Worker).ok,
               "Worker production has no extra tech prerequisite");
        expect(TechTreeValidator::canResearch(withTownHall, rts::core::data::UpgradeType::None).reason
               == LockReason::UnknownUpgrade,
               "Upgrade research reports UnknownUpgrade until upgrade content exists");
    }

    void testReplayLog() {
        namespace replay = rts::core::replay;
        namespace command = rts::core::command;

        replay::ReplayLog log;
        log.setMapPath(dataPath("maps/skirmish.json"));

        const command::MoveCommand move({ 128.0f, 256.0f }, true);
        auto serialized = replay::serializeLogicCommand(move);
        expect(serialized.has_value(), "MoveCommand serializes for replay");
        if (serialized) {
            log.record(3, *serialized);
        }
        log.checkpoint(30, 0xC0FFEEull);
        log.setMetadata("victory", 30);

        const auto path = std::filesystem::temp_directory_path() / "rts_headless_replay.json";
        expect(log.save(path.string()), "ReplayLog saves to temp file");

        replay::ReplayLog loaded;
        expect(loaded.load(path.string()), "ReplayLog loads from temp file");
        expect(loaded.mapPath() == dataPath("maps/skirmish.json"), "ReplayLog preserves map path");
        expect(loaded.lastTick() == 30, "ReplayLog preserves last tick from checkpoint");
        expect(loaded.result() == "victory", "ReplayLog preserves result metadata");
        expect(loaded.durationTicks() == 30, "ReplayLog preserves duration metadata");
        expect(loaded.hashForTick(30).value_or(0) == 0xC0FFEEull,
               "ReplayLog preserves hash checkpoint");

        const auto commands = loaded.commandsForTick(3);
        expect(commands.size() == 1, "ReplayLog returns command for recorded tick");
        if (!commands.empty()) {
            auto deserialized = replay::deserializeLogicCommand(commands.front()->cmd);
            const auto* deserializedMove =
                dynamic_cast<const command::MoveCommand*>(deserialized.get());
            expect(deserializedMove != nullptr, "ReplayLog deserializes MoveCommand");
            if (deserializedMove) {
                expect(deserializedMove->append(), "ReplayLog preserves append flag");
                expect(deserializedMove->target().x == 128.0f &&
                       deserializedMove->target().y == 256.0f,
                       "ReplayLog preserves move target");
            }
        }

        std::error_code ec;
        std::filesystem::remove(path, ec);
    }

    void testFixedMath() {
        using rts::core::sim::Fixed;
        using rts::core::sim::FixedVec2;
        const FixedVec2 next = rts::core::sim::stepToward(
            { Fixed::fromInt(0), Fixed::fromInt(0) },
            { Fixed::fromInt(0), Fixed::fromInt(8) },
            Fixed::fromInt(2));
        expect(next == FixedVec2 { Fixed::fromInt(0), Fixed::fromInt(2) },
               "Fixed stepToward advances deterministically");
    }
}

int main() {
    testDataRegistry();
    testMapLoading();
    testTechTree();
    testReplayLog();
    testFixedMath();

    if (g_failures != 0) {
        std::cerr << "[rts_headless_smoke] " << g_failures << " check(s) failed\n";
        return 1;
    }

    std::cout << "[rts_headless_smoke] all checks passed\n";
    return 0;
}
