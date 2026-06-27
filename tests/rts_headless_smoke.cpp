#include <filesystem>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>

#include "core/command/CommandRouterBase.hpp"
#include "core/command/LogicCommand.hpp"
#include "core/command/LogicCommandBus.hpp"
#include "core/data/DataPaths.hpp"
#include "core/data/DataRegistry.hpp"
#include "core/manager/CameraManager.hpp"
#include "core/map/MapData.hpp"
#include "core/model/Building.hpp"
#include "core/model/ResourceNode.hpp"
#include "core/model/Unit.hpp"
#include "core/path/IGridQuery.hpp"
#include "core/replay/ReplayLog.hpp"
#include "core/render/RenderQueue.hpp"
#include "core/sim/Fixed.hpp"
#include "core/sim/SimClock.hpp"
#include "core/tech/TechTreeValidator.hpp"
#include "core/world/GameWorld.hpp"
#include "game/game/GameLogicManager.hpp"
#include "game/game/GameUIManager.hpp"
#include "game/game/systems/CollisionSystem.hpp"

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

        // Portfolio showcase map (TMX): the CSV "collision" tile layer must load as
        // blocked tiles, and the object group must yield the full entity roster.
        const auto portfolio = rts::core::map::loadMap(dataPath("maps/portfolio.tmx"));
        expect(portfolio.width == 256 && portfolio.height == 256, "portfolio.tmx is 256x256");
        expect(portfolio.tileSize == 16.0f, "portfolio.tmx tile size is 16");
        expect(portfolio.buildings.size() == 5, "portfolio.tmx loads 5 buildings");
        expect(portfolio.units.size() == 28, "portfolio.tmx loads 28 units");
        expect(portfolio.resources.size() == 31, "portfolio.tmx loads 31 resources");
        expect(portfolio.blockedTiles.size() > 1000, "portfolio.tmx loads collision tiles");

        const float portfolioWorldW = static_cast<float>(portfolio.width) * portfolio.tileSize;
        const float portfolioWorldH = static_cast<float>(portfolio.height) * portfolio.tileSize;
        const auto inPortfolioBounds = [&](const rts::core::model::Vector2D& p) {
            return p.x >= 0.0f && p.y >= 0.0f &&
                   p.x <= portfolioWorldW && p.y <= portfolioWorldH;
        };
        const bool portfolioObjectsInBounds =
            std::all_of(portfolio.buildings.begin(), portfolio.buildings.end(),
                [&](const auto& b) { return inPortfolioBounds(b.position); }) &&
            std::all_of(portfolio.units.begin(), portfolio.units.end(),
                [&](const auto& u) { return inPortfolioBounds(u.position); }) &&
            std::all_of(portfolio.resources.begin(), portfolio.resources.end(),
                [&](const auto& r) { return inPortfolioBounds(r.position); });
        expect(portfolioObjectsInBounds, "portfolio.tmx entities stay inside map bounds");
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

    void testPortfolioGameTicks() {
        using clock = std::chrono::steady_clock;

        rts::core::command::LogicCommandBus bus;
        rts::core::command::LogicCommandRouter router;
        rts::core::world::GameWorld world;
        rts::core::manager::GameLogicManager logic(bus, router, world);

        {
            const auto lock = world.acquireReadLock();
            expect(world.gridWidth() == 256 && world.gridHeight() == 256,
                   "portfolio.tmx default match initializes 256x256 terrain");
            expect(world.getElements().size() == 64,
                   "portfolio.tmx default match spawns the full entity roster");
        }

        constexpr int ticks = 90;
        double maxTickMs = 0.0;
        double totalTickMs = 0.0;
        for (int i = 0; i < ticks; ++i) {
            const auto start = clock::now();
            logic.tick(rts::core::sim::kFixedDeltaSeconds);
            const auto elapsed = std::chrono::duration<double, std::milli>(
                clock::now() - start).count();
            maxTickMs = std::max(maxTickMs, elapsed);
            totalTickMs += elapsed;
        }

        {
            const auto lock = world.acquireReadLock();
            expect(world.currentTick() == ticks,
                   "portfolio.tmx default match advances simulation ticks");
        }

        std::cout << "[rts_headless_smoke] portfolio.tmx " << ticks
                  << " ticks max=" << maxTickMs
                  << "ms total=" << totalTickMs << "ms\n";

        // The old portfolio freeze held the write lock for multiple seconds inside
        // one logic tick, which made GameScene's read lock look deadlocked.
        expect(maxTickMs < 100.0, "portfolio.tmx tick stays under 100ms");
    }

    void testCameraBoundsFollowPortfolioMap() {
        rts::core::command::LogicCommandBus logicBus;
        rts::core::command::LogicCommandRouter logicRouter;
        rts::core::world::GameWorld world;
        rts::core::manager::GameLogicManager logic(logicBus, logicRouter, world);

        rts::core::command::UICommandRouter uiRouter;
        rts::core::render::RenderQueue renderQueue;
        rts::core::manager::CameraManager camera;
        rts::core::manager::GameUIManager ui(uiRouter, logicBus, renderQueue, world, camera);

        {
            const auto lock = world.acquireReadLock();
            ui.syncWithWorld();
        }

        const auto& worldSize = camera.worldSize();
        expect(std::abs(worldSize.x - 4096.0f) < 0.01f &&
               std::abs(worldSize.y - 4096.0f) < 0.01f,
               "game UI camera bounds follow portfolio.tmx world size");

        camera.setViewportSize({ 1920.0f, 1080.0f });
        camera.setPosition({ 4096.0f, 4096.0f });
        const auto& cameraPos = camera.position();
        expect(std::abs(cameraPos.x - 2176.0f) < 0.01f &&
               std::abs(cameraPos.y - 3016.0f) < 0.01f,
               "portfolio.tmx camera can pan to the lower map edge");

        const auto& grid = world.gridQuery();
        expect(grid.isBlockedStatic({ 35, 205 }),
               "portfolio.tmx building pathing keeps wider clearance around town hall");

        rts::core::manager::CollisionSystem collision;
        rts::core::model::Unit probe(rts::UnitType::Worker);
        expect(collision.canMoveUnitTo(world, probe, { 3000.0f, 3000.0f }),
               "collision bounds follow the loaded portfolio.tmx map size");
    }
}

int main() {
    testDataRegistry();
    testMapLoading();
    testTechTree();
    testReplayLog();
    testFixedMath();
    testPortfolioGameTicks();
    testCameraBoundsFollowPortfolioMap();

    if (g_failures != 0) {
        std::cerr << "[rts_headless_smoke] " << g_failures << " check(s) failed\n";
        return 1;
    }

    std::cout << "[rts_headless_smoke] all checks passed\n";
    return 0;
}
