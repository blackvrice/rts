#include "game/game/systems/MatchSnapshot.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/data/BuildingStaticData.hpp"
#include "core/data/ResourceStaticData.hpp"
#include "core/ecs/EntityId.hpp"
#include "core/model/Building.hpp"
#include "core/model/Projectile.hpp"
#include "core/model/ResourceNode.hpp"
#include "core/model/Unit.hpp"
#include "core/world/GameWorld.hpp"

namespace {
    // JSON <-> runtime-state helpers. Entity references are stored as the saved
    // EntityId and remapped to the newly allocated ids on load.
    std::uint64_t entityKey(const rts::core::ecs::EntityId id) {
        return (static_cast<std::uint64_t>(id.index) << 32) | id.generation;
    }

    nlohmann::json entityRef(const rts::core::ecs::EntityId id) {
        return nlohmann::json { { "index", id.index }, { "generation", id.generation } };
    }

    rts::core::ecs::EntityId entityIdFromJson(const nlohmann::json& j) {
        return rts::core::ecs::EntityId {
            j.value("index", rts::core::ecs::InvalidEntityId.index),
            j.value("generation", rts::core::ecs::InvalidEntityId.generation)
        };
    }

    rts::core::ecs::EntityId remapEntityId(
        const rts::core::ecs::EntityId oldId,
        const std::unordered_map<std::uint64_t, rts::core::ecs::EntityId>& remap) {
        if (!rts::core::ecs::isValid(oldId)) {
            return rts::core::ecs::InvalidEntityId;
        }
        if (const auto it = remap.find(entityKey(oldId)); it != remap.end()) {
            return it->second;
        }
        return rts::core::ecs::InvalidEntityId;
    }

    nlohmann::json vecJson(const rts::core::model::Vector2D& v) {
        return nlohmann::json { { "x", v.x }, { "y", v.y } };
    }

    rts::core::model::Vector2D vecFromJson(const nlohmann::json& j) {
        return { j.value("x", 0.0f), j.value("y", 0.0f) };
    }

    nlohmann::json orderJson(const rts::core::model::UnitOrder& order) {
        return nlohmann::json {
            { "type", static_cast<int>(order.type) },
            { "targetEntityId", entityRef(order.targetEntityId) },
            { "targetPosition", vecJson(order.targetPosition) },
            { "abilityId", order.abilityId },
            { "buildingTypeId", order.buildingTypeId }
        };
    }

    rts::core::model::UnitOrder orderFromJson(
        const nlohmann::json& j,
        const std::unordered_map<std::uint64_t, rts::core::ecs::EntityId>& remap) {
        rts::core::model::UnitOrder order {};
        order.type = static_cast<rts::core::model::OrderType>(j.value("type", 0));
        order.targetEntityId = remapEntityId(entityIdFromJson(j.value("targetEntityId", nlohmann::json::object())), remap);
        order.targetPosition = vecFromJson(j.value("targetPosition", nlohmann::json::object()));
        order.abilityId = j.value("abilityId", -1);
        order.buildingTypeId = j.value("buildingTypeId", -1);
        return order;
    }

    nlohmann::json unitRuntimeJson(const rts::core::model::Unit::RuntimeState& state) {
        nlohmann::json queue = nlohmann::json::array();
        for (const auto& order : state.orderQueue) {
            queue.push_back(orderJson(order));
        }

        return nlohmann::json {
            { "action", static_cast<int>(state.action) },
            { "animationAction", static_cast<int>(state.animationAction) },
            { "moveTarget", vecJson(state.moveTarget) },
            { "finalTargetWorld", vecJson(state.finalTargetWorld) },
            { "attackMoveTarget", vecJson(state.attackMoveTarget) },
            { "patrolStart", vecJson(state.patrolStart) },
            { "patrolEnd", vecJson(state.patrolEnd) },
            { "patrolDestination", vecJson(state.patrolDestination) },
            { "attackTargetId", entityRef(state.attackTargetId) },
            { "buildTargetId", entityRef(state.buildTargetId) },
            { "attackMoveActive", state.attackMoveActive },
            { "patrolActive", state.patrolActive },
            { "patrolHeadingToEnd", state.patrolHeadingToEnd },
            { "attackRetargetRequested", state.attackRetargetRequested },
            { "orderQueue", queue },
            { "carryingType", static_cast<int>(state.carryingType) },
            { "carryingAmount", state.carryingAmount },
            { "maxCarryAmount", state.maxCarryAmount },
            { "gatherProgressSeconds", state.gatherProgressSeconds },
            { "gatherPhase", static_cast<int>(state.gatherPhase) },
            { "deliveryReady", state.deliveryReady },
            { "targetResourceId", entityRef(state.targetResourceId) },
            { "targetDropOffId", entityRef(state.targetDropOffId) },
            { "attackPhase", static_cast<int>(state.attackPhase) },
            { "attackTimer", state.attackTimer },
            { "gridPath", [&state] {
                nlohmann::json path = nlohmann::json::array();
                for (const auto& cell : state.gridPath) {
                    path.push_back({ cell.x, cell.y });
                }
                return path;
            }() }
        };
    }

    rts::core::model::Unit::RuntimeState unitRuntimeFromJson(
        const nlohmann::json& j,
        const std::unordered_map<std::uint64_t, rts::core::ecs::EntityId>& remap) {
        rts::core::model::Unit::RuntimeState state {};
        state.action = static_cast<rts::core::model::ActionType>(j.value("action", 0));
        state.animationAction = static_cast<rts::core::model::ActionType>(j.value("animationAction", 0));
        state.moveTarget = vecFromJson(j.value("moveTarget", nlohmann::json::object()));
        state.finalTargetWorld = vecFromJson(j.value("finalTargetWorld", nlohmann::json::object()));
        state.attackMoveTarget = vecFromJson(j.value("attackMoveTarget", nlohmann::json::object()));
        state.patrolStart = vecFromJson(j.value("patrolStart", nlohmann::json::object()));
        state.patrolEnd = vecFromJson(j.value("patrolEnd", nlohmann::json::object()));
        state.patrolDestination = vecFromJson(j.value("patrolDestination", nlohmann::json::object()));
        state.attackTargetId = remapEntityId(entityIdFromJson(j.value("attackTargetId", nlohmann::json::object())), remap);
        state.buildTargetId = remapEntityId(entityIdFromJson(j.value("buildTargetId", nlohmann::json::object())), remap);
        state.attackMoveActive = j.value("attackMoveActive", false);
        state.patrolActive = j.value("patrolActive", false);
        state.patrolHeadingToEnd = j.value("patrolHeadingToEnd", true);
        state.attackRetargetRequested = j.value("attackRetargetRequested", false);
        for (const auto& order : j.value("orderQueue", nlohmann::json::array())) {
            state.orderQueue.push_back(orderFromJson(order, remap));
        }
        state.carryingType = static_cast<rts::core::model::ResourceNode::ResourceType>(j.value("carryingType", 0));
        state.carryingAmount = j.value("carryingAmount", 0);
        state.maxCarryAmount = j.value("maxCarryAmount", 10);
        state.gatherProgressSeconds = j.value("gatherProgressSeconds", 0.0f);
        state.gatherPhase = static_cast<rts::core::model::Unit::GatherPhase>(j.value("gatherPhase", 0));
        state.deliveryReady = j.value("deliveryReady", false);
        state.targetResourceId = remapEntityId(entityIdFromJson(j.value("targetResourceId", nlohmann::json::object())), remap);
        state.targetDropOffId = remapEntityId(entityIdFromJson(j.value("targetDropOffId", nlohmann::json::object())), remap);
        state.attackPhase = static_cast<rts::core::model::Unit::AttackPhase>(j.value("attackPhase", 0));
        state.attackTimer = j.value("attackTimer", 0.0f);
        for (const auto& cell : j.value("gridPath", nlohmann::json::array())) {
            if (cell.is_array() && cell.size() == 2) {
                state.gridPath.push_back({ cell[0].get<int>(), cell[1].get<int>() });
            }
        }
        return state;
    }

    nlohmann::json buildingRuntimeJson(const rts::core::model::Building::RuntimeState& state) {
        nlohmann::json queue = nlohmann::json::array();
        for (const auto type : state.trainQueue) {
            queue.push_back(static_cast<int>(type));
        }
        return nlohmann::json {
            { "queue", queue },
            { "trainTimer", state.trainTimer },
            { "rallyPoint", vecJson(state.rallyPoint) },
            { "hasRallyPoint", state.hasRallyPoint },
            { "completed", state.completed },
            { "buildTime", state.buildTime },
            { "buildProgress", state.buildProgress }
        };
    }

    rts::core::model::Building::RuntimeState buildingRuntimeFromJson(const nlohmann::json& j) {
        rts::core::model::Building::RuntimeState state {};
        for (const auto& type : j.value("queue", nlohmann::json::array())) {
            state.trainQueue.push_back(static_cast<rts::UnitType>(type.get<int>()));
        }
        state.trainTimer = j.value("trainTimer", 0.0f);
        state.rallyPoint = vecFromJson(j.value("rallyPoint", nlohmann::json::object()));
        state.hasRallyPoint = j.value("hasRallyPoint", false);
        state.completed = j.value("completed", true);
        state.buildTime = j.value("buildTime", 0.0f);
        state.buildProgress = j.value("buildProgress", 0.0f);
        return state;
    }
}

namespace rts::core::manager::snapshot {
    nlohmann::json capture(
        const world::GameWorld& world,
        const std::string& mapPath,
        const bool enemyAiPassive,
        const nlohmann::json& aiStates) {
        using json = nlohmann::json;
        json doc;
        doc["version"] = kSaveVersion;
        doc["localPlayer"] = world.localPlayerId();
        json slots = json::array();
        for (const auto& slot : world.players()) {
            slots.push_back({
                { "id", slot.id },
                { "alliance", slot.alliance },
                { "controller", static_cast<int>(slot.controller) },
                { "color", static_cast<int>(slot.color) },
                { "eliminated", slot.eliminated }
            });
        }
        doc["slots"] = slots;
        doc["map"] = mapPath;
        doc["enemyAiPassive"] = enemyAiPassive;
        doc["tick"] = static_cast<std::uint64_t>(world.currentTick());
        doc["worldHash"] = world.worldHash();
        doc["gameResult"] = static_cast<int>(world.gameResult());
        doc["ais"] = aiStates;

        json players = json::array();
        for (const auto& slot : world.players()) {
            const int team = slot.id;
            const auto& r = world.playerResources(team);
            players.push_back({
                { "team", team }, { "gold", r.gold }, { "wood", r.wood },
                { "foodUsed", r.foodUsed }, { "foodCapacity", r.foodCapacity }, { "army", r.army }
            });
        }
        doc["players"] = players;

        json entities = json::array();
        for (const auto& el : world.getElements()) {
            // Dead elements are saved too: they stay in the live world (hash order,
            // corpses, projectile targets), so dropping them would shift the
            // restored match.
            auto ge = std::dynamic_pointer_cast<model::IGameElement>(el);
            if (!ge) {
                continue;
            }
            const auto pos = ge->getPosition();
            json entity {
                { "saveId", entityRef(ge->entityId()) },
                { "position", vecJson(pos) },
                { "team", ge->getTeamId() }
            };
            if (auto u = std::dynamic_pointer_cast<model::Unit>(el)) {
                entity["kind"] = "unit";
                entity["type"] = static_cast<int>(u->unitType());
                entity["hp"] = u->getHp();
                entity["runtime"] = unitRuntimeJson(u->runtimeState());
            } else if (auto b = std::dynamic_pointer_cast<model::Building>(el)) {
                entity["kind"] = "building";
                entity["type"] = static_cast<int>(b->buildingType());
                entity["hp"] = b->getHp();
                entity["runtime"] = buildingRuntimeJson(b->runtimeState());
            } else if (auto rn = std::dynamic_pointer_cast<model::ResourceNode>(el)) {
                entity["kind"] = "resource";
                entity["type"] = static_cast<int>(rn->type());
                entity["totalAmount"] = rn->totalAmount();
                entity["remaining"] = rn->remaining();
                entity["gatherAmount"] = rn->gatherAmountPerTrip();
                entity["gatherDurationSeconds"] = rn->gatherDurationSeconds();
                entity["maxGatherers"] = rn->maxGatherers();
            } else {
                continue;
            }
            entities.push_back(std::move(entity));
        }
        doc["entities"] = entities;

        // EntityId allocator (generations, alive flags, free-list order), so the
        // load restores identical handles and future spawns get identical ids.
        const auto ids = world.entityState();
        json alive = json::array();
        for (const bool a : ids.alive) alive.push_back(a);
        doc["entityManager"] = {
            { "generations", ids.generations },
            { "alive", alive },
            { "free", ids.freeIndices }
        };

        // In-flight projectiles (targets by EntityId; dead targets stay resolvable
        // through the saved element list).
        json projectiles = json::array();
        for (const auto& projectile : world.projectiles()) {
            if (!projectile || projectile->expired()) continue;
            projectiles.push_back({
                { "position", vecJson(projectile->position()) },
                { "target", entityRef(projectile->target()
                                          ? projectile->target()->entityId()
                                          : ecs::InvalidEntityId) },
                { "lastKnownTarget", vecJson(projectile->lastKnownTargetPosition()) },
                { "angle", projectile->angleDeg() },
                { "damage", projectile->damage() },
                { "owner", projectile->ownerTeamId() },
                { "speed", projectile->speed() },
                { "weapon", static_cast<int>(projectile->weaponType()) },
                { "splash", { { "inner", projectile->splash().inner },
                              { "mid", projectile->splash().mid },
                              { "outer", projectile->splash().outer } } },
                { "texture", projectile->texturePath() }
            });
        }
        doc["projectiles"] = projectiles;
        return doc;
    }

    void restore(
        world::GameWorld& world,
        const nlohmann::json& doc,
        const std::function<void(model::Building&)>& onBuilding) {
        using json = nlohmann::json;
        if (doc.contains("slots")) {
            std::vector<model::PlayerSlot> slots;
            for (const auto& s : doc.value("slots", json::array())) {
                model::PlayerSlot slot;
                slot.id = s.value("id", model::TeamId::Neutral);
                slot.alliance = s.value("alliance", slot.id);
                slot.controller = static_cast<model::PlayerController>(s.value("controller", 1));
                slot.color = static_cast<model::PlayerColor>(
                    s.value("color", static_cast<int>(model::defaultPlayerColor(slot.id))));
                slot.eliminated = s.value("eliminated", false);
                slots.push_back(slot);
            }
            world.setPlayers(std::move(slots), doc.value("localPlayer", model::TeamId::Player));
        } else {
            // Pre-v3 saves were always human player 1 vs AI player 2.
            world.setPlayers({
                model::PlayerSlot { model::TeamId::Player, model::TeamId::Player,
                                    model::PlayerController::Human, model::PlayerColor::Blue, false },
                model::PlayerSlot { model::TeamId::Enemy, model::TeamId::Enemy,
                                    model::PlayerController::Ai, model::PlayerColor::Red, false }
            }, model::TeamId::Player);
        }

        for (const auto& p : doc.value("players", json::array())) {
            model::PlayerResourceState r {};
            r.gold = p.value("gold", r.gold);
            r.wood = p.value("wood", r.wood);
            r.foodUsed = p.value("foodUsed", 0);
            r.foodCapacity = p.value("foodCapacity", 0);
            r.army = p.value("army", 0);
            world.setPlayerResources(p.value("team", model::TeamId::Neutral), r);
        }

        std::unordered_map<std::uint64_t, ecs::EntityId> entityRemap;
        std::vector<std::pair<std::shared_ptr<model::Unit>, json>> pendingUnitRuntime;
        std::vector<std::pair<std::shared_ptr<model::Building>, json>> pendingBuildingRuntime;

        // v4 saves carry the allocator: restore exact ids instead of remapping.
        const bool exactIds = doc.contains("entityManager");
        if (exactIds) {
            const auto& em = doc["entityManager"];
            ecs::EntityManager::State ids;
            ids.generations = em.value("generations", std::vector<std::uint32_t> {});
            for (const auto& a : em.value("alive", json::array())) ids.alive.push_back(a.get<bool>());
            ids.freeIndices = em.value("free", std::vector<std::uint32_t> {});
            world.restoreEntityState(std::move(ids));
        }
        std::unordered_map<std::uint64_t, model::IGameElement*> elementById;

        if (doc.contains("entities")) {
            for (const auto& e : doc.value("entities", json::array())) {
                const std::string kind = e.value("kind", std::string {});
                const auto oldId = entityIdFromJson(e.value("saveId", json::object()));
                const auto pos = vecFromJson(e.value("position", json::object()));
                const int team = e.value("team", model::TeamId::Neutral);

                std::shared_ptr<model::IGameElement> created;
                if (kind == "building") {
                    const auto type = static_cast<model::BuildingType>(e.value("type", 0));
                    auto building = std::make_shared<model::Building>(type, pos, team);
                    onBuilding(*building);
                    building->setHp(e.value("hp", building->getMaxHp()));
                    created = building;
                    pendingBuildingRuntime.emplace_back(building, e.value("runtime", json::object()));
                } else if (kind == "unit") {
                    auto unit = std::make_shared<model::Unit>(
                        static_cast<::rts::UnitType>(e.value("type", 0)));
                    unit->setPosition(pos);
                    unit->setTeamId(team);
                    unit->setHp(e.value("hp", unit->getMaxHp()));
                    created = unit;
                    pendingUnitRuntime.emplace_back(unit, e.value("runtime", json::object()));
                } else if (kind == "resource") {
                    const auto type = static_cast<model::ResourceNode::ResourceType>(e.value("type", 0));
                    const auto data = core::data::resourceStaticDataFor(type);
                    auto node = std::make_shared<model::ResourceNode>(
                        pos,
                        type,
                        e.value("totalAmount", data.initialAmount),
                        e.value("gatherAmount", data.gatherAmountPerTrip),
                        e.value("gatherDurationSeconds", data.gatherDurationSeconds),
                        e.value("maxGatherers", data.maxGatherers));
                    node->setRemaining(e.value("remaining", data.initialAmount));
                    created = node;
                }

                if (created) {
                    if (exactIds) {
                        world.restoreElement(created, oldId);
                    } else {
                        world.addElement(created);
                    }
                    if (ecs::isValid(oldId)) {
                        entityRemap[entityKey(oldId)] = created->entityId();
                        elementById[entityKey(oldId)] = created.get();
                    }
                }
            }

            // Runtime state is restored after every entity exists so references
            // (attack/gather/build targets) can be remapped to the new ids.
            for (const auto& [building, runtime] : pendingBuildingRuntime) {
                building->restoreRuntimeState(buildingRuntimeFromJson(runtime));
            }
            for (const auto& [unit, runtime] : pendingUnitRuntime) {
                unit->restoreRuntimeState(unitRuntimeFromJson(runtime, entityRemap));
            }

            for (const auto& p : doc.value("projectiles", json::array())) {
                const auto targetId = entityIdFromJson(p.value("target", json::object()));
                model::IGameElement* target = nullptr;
                if (ecs::isValid(targetId)) {
                    if (const auto it = elementById.find(entityKey(targetId)); it != elementById.end()) {
                        target = it->second;
                    }
                }
                const auto splash = p.value("splash", json::object());
                world.restoreProjectile(
                    vecFromJson(p.value("position", json::object())),
                    target,
                    vecFromJson(p.value("lastKnownTarget", json::object())),
                    p.value("angle", 0.0f),
                    p.value("damage", 0.0f),
                    p.value("owner", model::TeamId::Neutral),
                    p.value("speed", 0.0f),
                    static_cast<data::WeaponType>(p.value("weapon", 0)),
                    data::SplashRadii { splash.value("inner", 0.0f),
                                        splash.value("mid", 0.0f),
                                        splash.value("outer", 0.0f) },
                    p.value("texture", std::string {}));
            }
            return;
        }

        // Pre-v2 saves: flat lists with visible state only.
        for (const auto& b : doc.value("buildings", json::array())) {
            const auto type = static_cast<model::BuildingType>(b.value("type", 0));
            const model::Vector2D pos { b.value("x", 0.0f), b.value("y", 0.0f) };
            const int team = b.value("team", model::TeamId::Neutral);
            auto building = std::make_shared<model::Building>(type, pos, team);
            onBuilding(*building);
            building->setHp(b.value("hp", building->getMaxHp()));
            if (!b.value("completed", true)) {
                const auto data = core::data::buildingStaticDataFor(type);
                building->beginConstruction(data.buildTimeSeconds, building->getHp());
            }
            world.addElement(building);
            for (const auto& q : b.value("queue", json::array())) {
                building->trainUnit(static_cast<::rts::UnitType>(q.get<int>()));
            }
        }

        for (const auto& u : doc.value("units", json::array())) {
            auto unit = std::make_shared<model::Unit>(
                static_cast<::rts::UnitType>(u.value("type", 0)));
            unit->setPosition({ u.value("x", 0.0f), u.value("y", 0.0f) });
            unit->setTeamId(u.value("team", model::TeamId::Neutral));
            unit->setHp(u.value("hp", unit->getMaxHp()));
            world.addElement(unit);
        }

        for (const auto& r : doc.value("resources", json::array())) {
            const auto data = core::data::resourceStaticDataFor(
                static_cast<model::ResourceNode::ResourceType>(r.value("type", 0)));
            const model::Vector2D pos { r.value("x", 0.0f), r.value("y", 0.0f) };
            auto node = std::make_shared<model::ResourceNode>(
                pos, data.resourceType, data.initialAmount,
                data.gatherAmountPerTrip, data.gatherDurationSeconds, data.maxGatherers);
            node->setRemaining(r.value("remaining", data.initialAmount));
            world.addElement(node);
        }
    }
}
