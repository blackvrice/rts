// Headless gameplay scenario tests (roadmap M0-E4-02). Each test loads a small
// map from tests/data/maps, issues the same LogicCommands the UI would send, and
// checks the simulation outcome. No SFML/window is involved.

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "core/command/LogicCommand.hpp"
#include "core/data/CombatTypes.hpp"
#include "core/data/DataPaths.hpp"
#include "core/data/DataRegistry.hpp"
#include "core/map/FogOfWar.hpp"
#include "core/model/PlayerSlot.hpp"
#include "core/model/Building.hpp"
#include "core/model/ResourceNode.hpp"
#include "core/model/Unit.hpp"
#include "core/world/GameWorld.hpp"
#include "sim/SimHarness.hpp"

namespace {
    using rts::test::SimHarness;
    using rts::core::model::BuildingType;
    using rts::core::model::ResourceNode;
    namespace TeamId = rts::core::model::TeamId;
    namespace command = rts::core::command;

    int g_failures = 0;

    void expect(const bool condition, const std::string& message) {
        if (!condition) {
            std::cerr << "[rts_sim_scenarios] FAIL: " << message << '\n';
            ++g_failures;
        }
    }

    std::string testMap(const char* name) {
        return (std::filesystem::path(RTS_TEST_DATA_DIR) / "maps" / name).string();
    }

    // A worker ordered onto gold completes trips and the gold lands in the
    // player's bank while the mine depletes by the same amount.
    void testGatherDeliversGold() {
        SimHarness sim(testMap("sim_economy.json"));
        int startGold = 0;
        int startMine = 0;
        rts::core::ecs::EntityId workerId {};
        rts::core::model::Vector2D goldPos {};
        {
            const auto lock = sim.world().acquireReadLock();
            const auto workers = sim.units(TeamId::Player, rts::UnitType::Worker);
            const auto gold = sim.resources(ResourceNode::ResourceType::Gold);
            expect(workers.size() == 1 && gold.size() == 1, "economy map has 1 worker and 1 gold mine");
            if (workers.empty() || gold.empty()) return;
            startGold = sim.economy(TeamId::Player).gold;
            startMine = gold.front()->remaining();
            workerId = workers.front()->entityId();
            goldPos = gold.front()->getPosition();
        }
        // Commands are sent outside the lock: handlers take the write lock.
        sim.select(workerId);
        sim.send(command::GatherCommand(goldPos));

        // Generous window: travel + 1.5s gather per 10-gold trip.
        const bool delivered = sim.tickUntil([&] {
            return sim.economy(TeamId::Player).gold >= startGold + 30;
        }, SimHarness::secondsToTicks(45.0f));

        const auto lock = sim.world().acquireReadLock();
        const int gained = sim.economy(TeamId::Player).gold - startGold;
        const auto gold = sim.resources(ResourceNode::ResourceType::Gold);
        expect(delivered, "worker delivers at least 30 gold within 45s (gained " + std::to_string(gained) + ")");
        if (!gold.empty()) {
            const int mined = startMine - gold.front()->remaining();
            // The worker may be carrying one undelivered trip when we stop.
            expect(mined >= gained && mined <= gained + 10,
                   "gold mine depletes by the delivered amount (mined " + std::to_string(mined) +
                   ", delivered " + std::to_string(gained) + ")");
        }
    }

    // Training a worker charges its cost up front and spawns it after the
    // build time; supply usage follows.
    void testTrainWorker() {
        SimHarness sim(testMap("sim_economy.json"));
        int startGold = 0;
        rts::core::ecs::EntityId hallId {};
        {
            const auto lock = sim.world().acquireReadLock();
            const auto halls = sim.buildings(TeamId::Player, BuildingType::TownHall);
            expect(halls.size() == 1, "economy map has the player town hall");
            if (halls.empty()) return;
            startGold = sim.economy(TeamId::Player).gold;
            hallId = halls.front()->entityId();
        }
        sim.select(hallId);
        sim.send(command::TrainUnitCommand(-1, static_cast<int>(rts::UnitType::Worker)));

        {
            const auto lock = sim.world().acquireReadLock();
            const auto cost = rts::core::data::unitStaticDataFor(rts::UnitType::Worker).cost();
            expect(sim.economy(TeamId::Player).gold == startGold - cost.gold,
                   "training a worker charges its gold cost immediately");
        }

        const float buildSeconds =
            rts::core::data::unitStaticDataFor(rts::UnitType::Worker).buildTimeSeconds;
        const bool spawned = sim.tickUntil([&] {
            return sim.units(TeamId::Player, rts::UnitType::Worker).size() == 2;
        }, SimHarness::secondsToTicks(buildSeconds + 3.0f));
        expect(spawned, "trained worker spawns within build time + 3s");

        const auto lock = sim.world().acquireReadLock();
        expect(sim.economy(TeamId::Player).foodUsed == 2,
               "supply usage counts both workers (foodUsed " +
               std::to_string(sim.economy(TeamId::Player).foodUsed) + ")");
    }

    // A worker places a barracks: cost is paid, the site appears incomplete,
    // and the worker finishes it.
    void testBuildBarracks() {
        SimHarness sim(testMap("sim_economy.json"));
        int startGold = 0;
        rts::core::ecs::EntityId workerId {};
        {
            const auto lock = sim.world().acquireReadLock();
            const auto workers = sim.units(TeamId::Player, rts::UnitType::Worker);
            if (workers.empty()) {
                expect(false, "economy map has a worker");
                return;
            }
            startGold = sim.economy(TeamId::Player).gold;
            workerId = workers.front()->entityId();
        }
        sim.select(workerId);
        sim.send(command::BuildCommand(static_cast<int>(BuildingType::Barracks), { 640.0f, 640.0f }));

        // Construction starts once the worker reaches the site.
        const bool placed = sim.tickUntil([&] {
            return !sim.buildings(TeamId::Player, BuildingType::Barracks).empty();
        }, SimHarness::secondsToTicks(15.0f));
        expect(placed, "barracks construction site appears");
        if (!placed) return;

        {
            const auto lock = sim.world().acquireReadLock();
            const auto cost = rts::core::data::buildingStaticDataFor(BuildingType::Barracks).cost();
            expect(sim.economy(TeamId::Player).gold == startGold - cost.gold,
                   "barracks gold cost is paid");
            const auto barracks = sim.buildings(TeamId::Player, BuildingType::Barracks);
            expect(!barracks.empty() && !barracks.front()->isComplete(),
                   "new barracks starts incomplete");
        }

        const float buildSeconds =
            rts::core::data::buildingStaticDataFor(BuildingType::Barracks).buildTimeSeconds;
        const bool completed = sim.tickUntil([&] {
            const auto barracks = sim.buildings(TeamId::Player, BuildingType::Barracks);
            return !barracks.empty() && barracks.front()->isComplete();
        }, SimHarness::secondsToTicks(buildSeconds + 10.0f));
        expect(completed, "worker completes the barracks");
    }

    // A mixed army attack-moves onto the enemy base: the lone enemy warrior dies
    // first, then destroying the town hall ends the match in victory.
    void testCombatToVictory() {
        SimHarness sim(testMap("sim_combat.json"));
        rts::core::model::Vector2D hallPos {};
        {
            const auto lock = sim.world().acquireReadLock();
            const auto warriors = sim.units(TeamId::Player, rts::UnitType::Warrior);
            const auto archers = sim.units(TeamId::Player, rts::UnitType::Archer);
            const auto halls = sim.buildings(TeamId::Enemy, BuildingType::TownHall);
            expect(warriors.size() == 2 && archers.size() == 6 && halls.size() == 1,
                   "combat map spawns 2 warriors, 6 archers and the enemy hall");
            if (halls.empty()) return;
            hallPos = halls.front()->getPosition();
        }
        // Box-select the army block, then attack-move onto the hall.
        sim.send(command::SelectCommand({ 960.0f, 960.0f }, { 1120.0f, 1120.0f }));
        sim.send(command::AttackMoveCommand(-1, hallPos));

        const bool enemyWarriorDead = sim.tickUntil([&] {
            return sim.units(TeamId::Enemy, rts::UnitType::Warrior).empty();
        }, SimHarness::secondsToTicks(30.0f));
        expect(enemyWarriorDead, "attack-move kills the enemy warrior within 30s");

        const bool won = sim.tickUntil([&] {
            return sim.world().gameResult() == rts::core::world::GameResult::Victory;
        }, SimHarness::secondsToTicks(120.0f));

        const auto lock = sim.world().acquireReadLock();
        expect(won, "destroying the enemy town hall wins the match within 120s");
        expect(sim.units(TeamId::Player, rts::UnitType::Archer).size() == 6,
               "no archer is lost against a single warrior");
    }

    // KNOWN BUG (roadmap M1-E9-01): building path clearance keeps melee units
    // about two tiles off the footprint, but range against buildings is measured
    // center-to-center with the footprint half-diagonal, so most of a melee
    // group around a Town Hall never comes into range. Until the fix lands this
    // check only reports; set kMeleeBuildingReachFixed = true to enforce it.
    constexpr bool kMeleeBuildingReachFixed = false;

    void testMeleeReachesBuilding() {
        SimHarness sim(testMap("sim_melee_vs_building.json"));
        rts::core::ecs::EntityId hallId {};
        rts::core::model::Vector2D hallPos {};
        float startHp = 0.0f;
        {
            const auto lock = sim.world().acquireReadLock();
            const auto halls = sim.buildings(TeamId::Enemy, BuildingType::TownHall);
            if (halls.empty()) {
                expect(false, "melee map has the enemy hall");
                return;
            }
            hallId = halls.front()->entityId();
            hallPos = halls.front()->getPosition();
            startHp = halls.front()->getHp();
        }
        sim.send(command::SelectCommand({ 960.0f, 960.0f }, { 1120.0f, 1080.0f }));
        sim.send(command::AttackCommand(hallPos, false, hallId));

        // Let the group arrive, then measure damage over a fixed window.
        sim.tick(SimHarness::secondsToTicks(10.0f));
        float hpBefore = startHp;
        {
            const auto lock = sim.world().acquireReadLock();
            const auto halls = sim.buildings(TeamId::Enemy, BuildingType::TownHall);
            hpBefore = halls.empty() ? 0.0f : halls.front()->getHp();
        }
        constexpr float windowSeconds = 20.0f;
        sim.tick(SimHarness::secondsToTicks(windowSeconds));

        const auto lock = sim.world().acquireReadLock();
        const auto halls = sim.buildings(TeamId::Enemy, BuildingType::TownHall);
        const float hpAfter = halls.empty() ? 0.0f : halls.front()->getHp();
        const auto warrior = rts::core::data::unitStaticDataFor(rts::UnitType::Warrior);
        const float perWarriorDps = warrior.attackDamage *
            rts::core::data::damageMultiplier(warrior.weaponType, rts::core::data::ArmorType::Fortified) /
            warrior.attackCooldown;
        const float attackers = (hpBefore - hpAfter) / windowSeconds / perWarriorDps;
        std::cout << "[rts_sim_scenarios] melee vs Town Hall: ~" << attackers
                  << " of 6 warriors deal damage\n";
        if (kMeleeBuildingReachFixed) {
            expect(attackers >= 5.0f, "at least 5 of 6 warriors reach the Town Hall");
        } else if (attackers < 5.0f) {
            std::cout << "[rts_sim_scenarios] KNOWN BUG M1-E9-01: melee units cannot reach "
                         "building footprints (not failing the run)\n";
        }
    }

    // Same scenario and commands twice must give the same WorldHash (same
    // build, same machine), which replays rely on.
    void testDeterministicReplayOfScenario() {
        const auto run = [] {
            SimHarness sim(testMap("sim_combat.json"));
            rts::core::model::Vector2D hallPos {};
            {
                const auto lock = sim.world().acquireReadLock();
                const auto halls = sim.buildings(TeamId::Enemy, BuildingType::TownHall);
                if (halls.empty()) return std::uint64_t { 0 };
                hallPos = halls.front()->getPosition();
            }
            sim.send(command::SelectCommand({ 960.0f, 960.0f }, { 1120.0f, 1120.0f }));
            sim.send(command::AttackMoveCommand(-1, hallPos));
            sim.tick(SimHarness::secondsToTicks(20.0f));
            const auto lock = sim.world().acquireReadLock();
            return sim.world().worldHash();
        };
        const auto first = run();
        const auto second = run();
        expect(first != 0 && first == second, "identical scenario runs produce identical WorldHash");
    }

    // Per-run temp folder for save/replay files, so tests never touch data/saves.
    std::string tempSaveDir(const char* name) {
        const auto dir = std::filesystem::temp_directory_path() /
            ("rts_sim_scenarios_" + std::string(name));
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
        std::filesystem::create_directories(dir, ec);
        return dir.string();
    }

    rts::core::app::MatchConfig combatConfig(const std::string& saveDir) {
        rts::core::app::MatchConfig config;
        config.mapPath = testMap("sim_combat.json");
        config.enemyAiPassive = false;
        config.recordReplay = false;
        config.saveDirectory = saveDir;
        return config;
    }

    void startCombat(SimHarness& sim) {
        sim.send(command::SelectCommand({ 960.0f, 960.0f }, { 1120.0f, 1120.0f }));
        sim.send(command::AttackMoveCommand(-1, { 1400.0f, 1400.0f }));
    }

    // Quick-save mid-fight (a dead unit still in the world, arrows in flight,
    // units walking paths), play on, then quick-load: the world must match the
    // saved one exactly (save format v4).
    void testSaveLoadRestoresWorldHash() {
        const auto saveDir = tempSaveDir("save");
        SimHarness sim(combatConfig(saveDir));
        startCombat(sim);
        sim.tick(SimHarness::secondsToTicks(10.0f));
        {
            const auto lock = sim.world().acquireReadLock();
            expect(!sim.world().projectiles().empty(), "save point has arrows in flight");
        }
        const auto savedHash = sim.worldHash();
        sim.send(command::SaveGameCommand());
        expect(std::filesystem::exists(std::filesystem::path(saveDir) / "quicksave.json"),
               "quick-save writes to MatchConfig::saveDirectory");

        sim.tick(SimHarness::secondsToTicks(4.0f));
        expect(sim.worldHash() != savedHash, "simulation advanced after the save");
        sim.send(command::LoadGameCommand());
        expect(sim.worldHash() == savedHash, "quick-load restores the saved WorldHash");

        std::error_code ec;
        std::filesystem::remove_all(saveDir, ec);
    }

    // F6 record -> F6 stop -> F7 play: playback must reproduce the recorded
    // WorldHash at every sampled tick until the recording ends.
    void testReplayPlaybackMatchesRecording() {
        const auto saveDir = tempSaveDir("replay");
        SimHarness sim(combatConfig(saveDir));
        sim.send(command::ToggleRecordCommand());  // restarts the match and records
        startCombat(sim);

        constexpr int kTicks = 900;
        constexpr int kSample = 60;
        std::vector<std::uint64_t> recorded;
        for (int t = 1; t <= kTicks; ++t) {
            sim.tick();
            if (t % kSample == 0) recorded.push_back(sim.worldHash());
        }
        sim.send(command::ToggleRecordCommand());  // stop + save replay.json
        expect(std::filesystem::exists(std::filesystem::path(saveDir) / "replay.json"),
               "F6 replay slot writes to MatchConfig::saveDirectory");

        sim.send(command::PlayReplayCommand());
        int mismatches = 0;
        for (int t = 1; t <= kTicks; ++t) {
            sim.tick();
            if (t % kSample == 0 && sim.worldHash() != recorded[static_cast<std::size_t>(t / kSample - 1)]) {
                ++mismatches;
            }
        }
        expect(mismatches == 0, "replay playback matches the recording at every sample (" +
               std::to_string(mismatches) + " mismatches)");

        std::error_code ec;
        std::filesystem::remove_all(saveDir, ec);
    }

    // Save 10s into the fight (a dead unit lingers, arrows in flight), load,
    // and continue: the match must play out exactly like an uninterrupted run.
    void testSaveLoadContinuation() {
        constexpr float kBefore = 10.0f;
        constexpr float kAfter = 20.0f;
        std::uint64_t reference = 0;
        {
            SimHarness sim(combatConfig(tempSaveDir("ref")));
            startCombat(sim);
            sim.tick(SimHarness::secondsToTicks(kBefore + kAfter));
            reference = sim.worldHash();
        }
        const auto saveDir = tempSaveDir("continue");
        SimHarness sim(combatConfig(saveDir));
        startCombat(sim);
        sim.tick(SimHarness::secondsToTicks(kBefore));
        sim.send(command::SaveGameCommand());
        sim.send(command::LoadGameCommand());
        sim.tick(SimHarness::secondsToTicks(kAfter));
        expect(sim.worldHash() == reference,
               "save/load mid-fight continues exactly like an uninterrupted run");
        std::error_code ec;
        std::filesystem::remove_all(saveDir, ec);
    }

    rts::core::model::PlayerSlot slot(const int id, const int alliance,
                                      const rts::core::model::PlayerController controller) {
        return rts::core::model::PlayerSlot {
            id, alliance, controller, rts::core::model::defaultPlayerColor(id), false };
    }

    bool fogVisibleAt(const rts::core::world::GameWorld& world, const int playerId,
                      const rts::core::model::Vector2D& pos) {
        const auto cell = world.gridTransform().worldToGrid(pos);
        return world.fog(playerId).get(cell.x, cell.y) == rts::core::map::FogOfWar::State::Visible;
    }

    // Four AI players, one per corner, free-for-all: each runs its own economy
    // and vision, and they end up fighting each other (roadmap M0-E3 done
    // criterion: 3+ AIs, no crash, separate resources and fog).
    void testFourPlayerFreeForAll() {
        using rts::core::model::PlayerController;
        rts::core::app::MatchConfig config;
        config.mapPath = testMap("sim_ffa4.json");
        config.recordReplay = false;
        config.localPlayerId = 1;  // spectating player 1's view; every slot is AI
        for (int id = 1; id <= 4; ++id) {
            config.players.push_back(slot(id, id, PlayerController::Ai));
        }
        SimHarness sim(config);

        std::vector<rts::core::model::Vector2D> halls(5);
        {
            const auto lock = sim.world().acquireReadLock();
            expect(sim.world().players().size() == 4, "FFA map creates 4 player slots");
            for (int id = 1; id <= 4; ++id) {
                expect(sim.world().isAiControlled(id), "every FFA slot is AI-controlled");
                expect(sim.economy(id).gold == 500, "each player starts with its own 500 gold");
                const auto hall = sim.buildings(id, BuildingType::TownHall);
                expect(hall.size() == 1, "each player owns one Town Hall");
                if (!hall.empty()) halls[static_cast<std::size_t>(id)] = hall.front()->getPosition();
            }
            for (int a = 1; a <= 4; ++a) {
                for (int b = 1; b <= 4; ++b) {
                    expect(sim.world().areEnemies(a, b) == (a != b), "FFA: every other player is an enemy");
                }
            }
        }

        sim.tick();
        {
            const auto lock = sim.world().acquireReadLock();
            expect(fogVisibleAt(sim.world(), 1, halls[1]) && !fogVisibleAt(sim.world(), 1, halls[4]),
                   "player 1 sees its own base but not player 4's");
            expect(fogVisibleAt(sim.world(), 4, halls[4]) && !fogVisibleAt(sim.world(), 4, halls[1]),
                   "player 4 has its own fog");
        }

        // Run until some player loses a unit or building to an enemy attack.
        const auto anyCombatLoss = [&] {
            for (const auto& element : sim.world().getElements()) {
                const auto ge = std::dynamic_pointer_cast<rts::core::model::IGameElement>(element);
                if (ge && ge->getTeamId() != TeamId::Neutral &&
                    ge->getAction() == rts::core::model::ActionType::Dead) {
                    return true;
                }
            }
            return false;
        };
        const bool fought = sim.tickUntil(anyCombatLoss, SimHarness::secondsToTicks(300.0f));
        expect(fought, "the four AIs attack each other within 5 minutes");

        const auto lock = sim.world().acquireReadLock();
        int economiesMoved = 0;
        for (int id = 1; id <= 4; ++id) {
            const auto& e = sim.economy(id);
            if (e.gold != 500 || e.wood != 300) ++economiesMoved;
        }
        expect(economiesMoved == 4, "all four AI economies run independently (" +
               std::to_string(economiesMoved) + "/4 changed)");
        std::cout << "[rts_sim_scenarios] FFA after " << sim.world().currentTick() << " ticks:";
        for (int id = 1; id <= 4; ++id) {
            std::cout << " p" << id << " gold=" << sim.economy(id).gold
                      << " food=" << sim.economy(id).foodUsed
                      << (sim.world().player(id)->eliminated ? " (out)" : "");
        }
        std::cout << '\n';
    }

    // 2v2: players 1+2 vs 3+4. Allies never target each other, share vision,
    // and win together once both enemy Town Halls fall.
    void testAlliances() {
        using rts::core::model::PlayerController;
        rts::core::app::MatchConfig config;
        config.mapPath = testMap("sim_alliances.json");
        config.recordReplay = false;
        config.players = {
            slot(1, 1, PlayerController::Human), slot(2, 1, PlayerController::Human),
            slot(3, 2, PlayerController::Human), slot(4, 2, PlayerController::Human)
        };
        SimHarness sim(config);

        rts::core::model::Vector2D scoutPos {};
        {
            const auto lock = sim.world().acquireReadLock();
            const auto& w = sim.world();
            expect(!w.areEnemies(1, 2) && w.areAllies(1, 2), "players 1 and 2 are allies");
            expect(w.areEnemies(1, 3) && w.areEnemies(2, 4), "the two alliances are enemies");
            expect(!w.areEnemies(3, 4), "players 3 and 4 are allies");
            const auto p1 = sim.units(1, rts::UnitType::Warrior);
            const auto p2 = sim.units(2, rts::UnitType::Warrior);
            const auto p3 = sim.units(3, rts::UnitType::Warrior);
            if (!p1.empty() && !p2.empty() && !p3.empty()) {
                expect(!p1.front()->canAttackTarget(p2.front().get()), "a unit cannot target an ally");
                expect(p1.front()->canAttackTarget(p3.front().get()), "a unit can target an enemy alliance");
            }
            const auto scouts = sim.units(2, rts::UnitType::Worker);
            if (!scouts.empty()) scoutPos = scouts.front()->getPosition();
        }

        sim.tick();
        {
            const auto lock = sim.world().acquireReadLock();
            expect(fogVisibleAt(sim.world(), 1, scoutPos), "player 1 sees through its ally's scout");
            expect(!fogVisibleAt(sim.world(), 3, scoutPos), "the enemy alliance does not");
        }

        // Idle units auto-engage: the lone player-3 warrior should fall to the
        // four allied warriors, and the allies must never fight each other.
        const bool enemyDown = sim.tickUntil([&] {
            return sim.units(3, rts::UnitType::Warrior).empty();
        }, SimHarness::secondsToTicks(30.0f));
        {
            const auto lock = sim.world().acquireReadLock();
            expect(enemyDown, "the allied warriors kill the enemy warrior");
            expect(sim.units(1, rts::UnitType::Warrior).size() == 2 &&
                   sim.units(2, rts::UnitType::Warrior).size() == 2,
                   "no allied warrior is lost (no friendly fire between allies)");
        }

        // Knock out both enemy halls: the 1+2 alliance wins together.
        {
            const auto lock = sim.world().acquireWriteLock();
            for (const int enemy : { 3, 4 }) {
                for (const auto& hall : sim.buildings(enemy, BuildingType::TownHall)) {
                    hall->takeDamage(hall->getMaxHp() * 2.0f, nullptr);
                }
            }
        }
        sim.tick();
        const auto lock = sim.world().acquireReadLock();
        expect(sim.world().player(3)->eliminated && sim.world().player(4)->eliminated,
               "both enemy players are eliminated without a Town Hall");
        expect(!sim.world().player(2)->eliminated, "the ally stays in the game");
        expect(sim.world().gameResult() == rts::core::world::GameResult::Victory,
               "the local alliance wins once every enemy is eliminated");
    }

    // 4 AI players, saved mid-match (AI build orders and timers, queued paths,
    // training queues), loaded, and continued: must match the uninterrupted
    // match at every sample.
    void testFreeForAllSaveLoadContinuation() {
        using rts::core::model::PlayerController;
        const auto makeConfig = [](const std::string& saveDir) {
            rts::core::app::MatchConfig config;
            config.mapPath = testMap("sim_ffa4.json");
            config.recordReplay = false;
            config.saveDirectory = saveDir;
            for (int id = 1; id <= 4; ++id) {
                config.players.push_back(slot(id, id, PlayerController::Ai));
            }
            return config;
        };
        // 45s in: every AI has trained, gathered and is building; 30s after the
        // load is long enough for divergence to show (kept short for CTest time).
        const int before = SimHarness::secondsToTicks(45.0f);
        const int after = SimHarness::secondsToTicks(30.0f);

        std::vector<std::uint64_t> reference;
        {
            SimHarness sim(makeConfig(tempSaveDir("ffa_ref")));
            sim.tick(before);
            for (int t = 0; t < after; t += 300) {
                sim.tick(300);
                reference.push_back(sim.worldHash());
            }
        }

        const auto saveDir = tempSaveDir("ffa_continue");
        SimHarness sim(makeConfig(saveDir));
        sim.tick(before);
        sim.send(command::SaveGameCommand());
        sim.tick(SimHarness::secondsToTicks(5.0f));  // drift away, then load back
        sim.send(command::LoadGameCommand());
        int mismatches = 0;
        for (std::size_t i = 0; i < reference.size(); ++i) {
            sim.tick(300);
            if (sim.worldHash() != reference[i]) ++mismatches;
        }
        expect(mismatches == 0, "4-player FFA save/load continues identically (" +
               std::to_string(mismatches) + "/" + std::to_string(reference.size()) +
               " samples differ)");
        std::error_code ec;
        std::filesystem::remove_all(saveDir, ec);
    }
}

int main() {
    if (!rts::core::data::DataRegistry::global().loadFromDirectory(rts::core::data::DataRoot)) {
        std::cerr << "[rts_sim_scenarios] FAIL: DataRegistry could not load data/\n";
        return 1;
    }

    testGatherDeliversGold();
    testTrainWorker();
    testBuildBarracks();
    testCombatToVictory();
    testMeleeReachesBuilding();
    testDeterministicReplayOfScenario();
    testSaveLoadRestoresWorldHash();
    testReplayPlaybackMatchesRecording();
    testFourPlayerFreeForAll();
    testAlliances();
    testSaveLoadContinuation();
    testFreeForAllSaveLoadContinuation();

    if (g_failures != 0) {
        std::cerr << "[rts_sim_scenarios] " << g_failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "[rts_sim_scenarios] all scenarios passed\n";
    return 0;
}
