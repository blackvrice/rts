//
// Created by black on 26. 1. 1..
//

#include "game/game/GameUIManager.hpp"

#include <chrono>
#include <utility>

#include "core/command/CommandRouterBase.hpp"
#include "core/command/LogicCommand.hpp"
#include "core/command/LogicCommandBus.hpp"
#include "core/command/UICommand.hpp"
#include "core/manager/CameraManager.hpp"
#include "core/model/IElement.hpp"
#include "core/model/IGameElement.hpp"
#include "core/model/PlayerResourceState.hpp"
#include "core/model/Unit.hpp"
#include "core/model/Building.hpp"
#include "core/model/ResourceNode.hpp"
#include "core/data/BuildingStaticData.hpp"
#include "core/data/UnitStaticData.hpp"
#include "core/render/RenderCommand.hpp"
#include "core/render/RenderQueue.hpp"
#include "core/ui/IUIElement.hpp"
#include "core/ui/SelectBox.hpp"
#include "core/ui/TextBox.hpp"
#include "core/model/IViewModel.hpp"
#include "core/viewmodel/UnitViewModel.hpp"
#include "core/viewmodel/BuildingViewModel.hpp"
#include "core/viewmodel/ResourceNodeViewModel.hpp"
#include "core/viewmodel/ProjectileViewModel.hpp"
#include "core/model/Projectile.hpp"
#include "core/world/GameWorld.hpp"
#include "game/game/GameLogicManager.hpp"

namespace rts::core::manager {
    namespace {
        constexpr float kCameraStep = 128.0f;
        constexpr float kEdgeThreshold = 20.0f;
        constexpr float kEdgeScrollSpeed = 8.0f;

        const char* actionText(const core::model::ActionType action) {
            switch (action) {
                case core::model::ActionType::Idle:
                    return "Idle";
                case core::model::ActionType::Move:
                    return "Moving";
                case core::model::ActionType::Attack:
                    return "Attacking";
                case core::model::ActionType::Stop:
                    return "Stopped";
                case core::model::ActionType::Hold:
                    return "Holding";
                case core::model::ActionType::Patrol:
                    return "Patrolling";
                case core::model::ActionType::Gather:
                    return "Gathering";
                case core::model::ActionType::Build:
                    return "Building";
                case core::model::ActionType::Cast:
                    return "Casting";
                case core::model::ActionType::Dead:
                    return "Dead";
            }

            return "Unknown";
        }

        bool usesCommandModifier(const core::model::KeyModifier modifier) {
            using KM = core::model::KeyModifier;
            return static_cast<bool>(modifier & KM::Ctrl) ||
                   static_cast<bool>(modifier & KM::Alt) ||
                   static_cast<bool>(modifier & KM::System);
        }

        bool isShiftKey(const core::model::Key key) {
            return key == core::model::Key::LShift ||
                   key == core::model::Key::RShift;
        }

        bool isControlKey(const core::model::Key key) {
            return key == core::model::Key::LControl ||
                   key == core::model::Key::RControl;
        }

        bool starCraftHotkeyAction(
            const core::model::Key key,
            command::GameplayInputAction& action) {
            using core::model::Key;
            using command::GameplayInputAction;

            switch (key) {
                case Key::M:
                    action = GameplayInputAction::Move;
                    return true;
                case Key::A:
                    // A is attack-move (move to a point, engaging foes en route).
                    // Attacking a specific target is the default right-click.
                    action = GameplayInputAction::AttackMove;
                    return true;
                case Key::S:
                    action = GameplayInputAction::Stop;
                    return true;
                case Key::H:
                    action = GameplayInputAction::HoldPosition;
                    return true;
                case Key::P:
                    action = GameplayInputAction::Patrol;
                    return true;
                case Key::G:
                    action = GameplayInputAction::Gather;
                    return true;
                case Key::B:
                    action = GameplayInputAction::Build;
                    return true;
                case Key::R:
                    action = GameplayInputAction::Repair;
                    return true;
                case Key::T:
                    action = GameplayInputAction::TrainUnit;
                    return true;
                case Key::C:
                    action = GameplayInputAction::CancelProduction;
                    return true;
                default:
                    return false;
            }
        }
    }

    GameUIManager::~GameUIManager() = default;

    GameUIManager::GameUIManager(command::UICommandRouter &router, command::LogicCommandBus &logicBus,
                                 core::render::RenderQueue &render_queue, world::GameWorld& world,
                                 CameraManager& camera) : m_world(world), m_camera(camera), IUIManager(
        router, logicBus, render_queue) {

        router.on<command::MouseLeftPressedCommand>(
            [this](const command::MouseLeftPressedCommand &cmd) {
                m_isDragging = true;
                const core::model::Vector2D &pos = cmd.position();

                // Ctrl-click or a quick double-click on the same spot arms
                // "select all of this type" for the release that follows.
                const auto now = std::chrono::steady_clock::now();
                const bool quick = (now - m_lastClickTime) < std::chrono::milliseconds(350);
                const bool nearLast = pos.distanceTo(m_lastClickPos) < 16.0f;
                m_selectSameType = m_ctrl || (quick && nearLast);
                m_lastClickTime = now;
                m_lastClickPos = pos;

                for (const auto &element: m_elements) {
                    element->MouseDown(pos);
                }
            }
        );
        router.on<command::MouseMoveCommand>(
            [this](const command::MouseMoveCommand &cmd) {
                const core::model::Vector2D &pos = cmd.position();
                m_mousePos = pos;
                m_hasMousePos = true;
                for (const auto &element: m_elements) {
                    element->MouseMove(pos);
                }
            }
        );
        router.on<command::MouseLeftReleasedCommand>(
            [this](const command::MouseLeftReleasedCommand &cmd) {
                m_isDragging = false;
                const core::model::Vector2D &pos = cmd.position();
                for (const auto &element: m_elements) {
                    element->MouseUp(pos);
                }
            }
        );

        router.on<command::MouseRightPressedCommand>(
            [this](const command::MouseRightPressedCommand &cmd) {
                issueWorldOrder(cmd.position());
            }
        );

        router.on<command::GameplayInputCommand>(
            [this](const command::GameplayInputCommand &cmd) {
                handleGameplayInput(cmd);
            }
        );

        router.on<command::KeyPressedCommand>(
            [this](const command::KeyPressedCommand &cmd) {
                const core::model::Key &key = cmd.getCode();
                using KM = core::model::KeyModifier;
                const KM &modifier = cmd.getMods();
                if (isControlKey(key) || static_cast<bool>(modifier & KM::Ctrl)) m_ctrl = true;
                if (isShiftKey(key) || static_cast<bool>(modifier & KM::Shift)) m_shift = true;

                // Result screen: Enter restarts the match; all other input is inert.
                if (m_world.gameResult() != core::world::GameResult::InProgress) {
                    if (key == core::model::Key::Enter) {
                        m_logicBus.push(std::make_unique<command::RestartCommand>());
                    }
                    return;
                }

                command::GameplayInputAction hotkeyAction {};
                // StarCraft-style command hotkeys mirror the HUD command buttons
                // so both input paths stay aligned with the same LogicCommand bridge.
                if (!usesCommandModifier(modifier) && starCraftHotkeyAction(key, hotkeyAction)) {
                    const command::GameplayInputCommand hotkeyCommand(hotkeyAction);
                    handleGameplayInput(hotkeyCommand);
                    return;
                }

                switch (key) {
                    case core::model::Key::Escape:
                        // Cancel an armed build placement back to the default mode.
                        m_worldOrderMode = WorldOrderMode::Attack;
                        break;
                    case core::model::Key::Left:
                        m_camera.moveBy({-kCameraStep, 0.0f});
                        break;
                    case core::model::Key::Right:
                        m_camera.moveBy({kCameraStep, 0.0f});
                        break;
                    case core::model::Key::Up:
                        m_camera.moveBy({0.0f, -kCameraStep});
                        break;
                    case core::model::Key::Down:
                        m_camera.moveBy({0.0f, kCameraStep});
                        break;
                    case core::model::Key::Num1:
                    case core::model::Key::Num2:
                    case core::model::Key::Num3:
                    case core::model::Key::Num4:
                    case core::model::Key::Num5:
                    case core::model::Key::Num6:
                    case core::model::Key::Num7:
                    case core::model::Key::Num8:
                    case core::model::Key::Num9:
                    case core::model::Key::Num0:
                    {

                        uint16_t num = static_cast<uint16_t>(key - core::model::Key::Num1);
                        if (static_cast<bool>(modifier & KM::Ctrl) && static_cast<bool>(modifier & KM::Shift)) {
                        }
                        else if (static_cast<bool>(modifier & KM::Ctrl)) {
                            m_logicBus.push(std::make_unique<command::ControlGroupAssignCommand>(num));
                        }
                        else if (static_cast<bool>(modifier & KM::Shift)) {
                            m_logicBus.push(std::make_unique<command::ControlGroupAddCommand>(num));
                        }
                        else {
                            m_logicBus.push(std::make_unique<command::ControlGroupSelectCommand>(num));
                        }
                        break;
                    }
                }
            }
        );

        router.on<command::KeyReleasedCommand>([this](const command::KeyReleasedCommand &cmd) {
            const core::model::Key &key = cmd.getCode();
            using KM = core::model::KeyModifier;
            const KM &modifier = cmd.getMods();
            if (isControlKey(key)) {
                m_ctrl = static_cast<bool>(modifier & KM::Ctrl);
            } else if (!static_cast<bool>(modifier & KM::Ctrl)) {
                m_ctrl = false;
            }
            if (isShiftKey(key)) {
                m_shift = static_cast<bool>(modifier & KM::Shift);
            } else if (!static_cast<bool>(modifier & KM::Shift)) {
                m_shift = false;
            }
        });
        m_elements.push_back(std::make_unique<core::ui::SelectBox>(
            logicBus, m_camera, m_shift, m_selectSameType));
    }

    void GameUIManager::issueWorldOrder(const core::model::Vector2D& screenPosition) {
        if (m_world.gameResult() != core::world::GameResult::InProgress) {
            return;
        }
        const core::model::Vector2D worldPos = m_camera.screenToWorld(screenPosition);

        switch (m_worldOrderMode) {
            case WorldOrderMode::Move:
                m_logicBus.push(std::make_unique<command::MoveCommand>(worldPos, m_shift));
                break;
            case WorldOrderMode::Attack: {
                // Name the element under the cursor by EntityId so the command
                // targets that exact entity; the logic still validates it and
                // falls back to positional resolution when needed.
                core::ecs::EntityId targetId = core::ecs::InvalidEntityId;
                {
                    const auto lock = m_world.acquireReadLock();
                    float bestDist = 64.0f;
                    for (const auto& element : m_world.getElements()) {
                        auto ge = std::dynamic_pointer_cast<core::model::IGameElement>(element);
                        if (!ge || ge->getAction() == core::model::ActionType::Dead) {
                            continue;
                        }
                        const float dist = ge->getPosition().distanceTo(worldPos);
                        if (dist < bestDist) {
                            bestDist = dist;
                            targetId = ge->entityId();
                        }
                    }
                }
                m_logicBus.push(std::make_unique<command::AttackCommand>(worldPos, m_shift, targetId));
                break;
            }
            case WorldOrderMode::AttackMove:
                m_logicBus.push(std::make_unique<command::AttackMoveCommand>(-1, worldPos));
                break;
            case WorldOrderMode::Patrol:
                m_logicBus.push(std::make_unique<command::PatrolCommand>(
                    -1, core::model::Vector2D{}, worldPos));
                break;
            case WorldOrderMode::Gather:
                m_logicBus.push(std::make_unique<command::GatherCommand>(worldPos));
                break;
            case WorldOrderMode::Build:
                // Place the worker's default structure (Barracks) at the cursor.
                m_logicBus.push(std::make_unique<command::BuildCommand>(
                    static_cast<int>(core::model::BuildingType::Barracks), worldPos));
                break;
        }

        m_worldOrderMode = WorldOrderMode::Attack;
    }

    void GameUIManager::handleGameplayInput(const command::GameplayInputCommand& cmd) {
        if (m_world.gameResult() != core::world::GameResult::InProgress) {
            return;
        }
        switch (cmd.action()) {
            case command::GameplayInputAction::Move:
                m_worldOrderMode = WorldOrderMode::Move;
                break;
            case command::GameplayInputAction::Attack:
            case command::GameplayInputAction::AttackMove:
                m_worldOrderMode = WorldOrderMode::AttackMove;
                break;
            case command::GameplayInputAction::Gather:
                m_worldOrderMode = WorldOrderMode::Gather;
                break;
            case command::GameplayInputAction::Stop:
                m_logicBus.push(std::make_unique<command::StopCommand>(-1));
                break;
            case command::GameplayInputAction::HoldPosition:
                m_logicBus.push(std::make_unique<command::HoldPositionCommand>(-1));
                break;
            case command::GameplayInputAction::CancelProduction:
                m_logicBus.push(std::make_unique<command::CancelProductionCommand>(-1));
                break;
            case command::GameplayInputAction::TrainUnit:
                // buildingId -1 = use current selection, unitTypeId -1 = the building's
                // default unit. The logic layer resolves both against the selection.
                m_logicBus.push(std::make_unique<command::TrainUnitCommand>(-1, -1));
                break;
            case command::GameplayInputAction::Build:
                // Arm build placement; the next right-click resolves the location.
                m_worldOrderMode = WorldOrderMode::Build;
                break;
            case command::GameplayInputAction::Patrol:
                m_worldOrderMode = WorldOrderMode::Patrol;
                break;
            case command::GameplayInputAction::ReturnResource:
            case command::GameplayInputAction::Repair:
            case command::GameplayInputAction::UseAbility:
                // These need selected unit, target, or ability ids that the HUD does not provide yet.
                break;
        }
    }


    void GameUIManager::update() {
        if (m_hasMousePos) {
            const auto& vp = m_camera.viewportSize();
            core::model::Vector2D delta{0.0f, 0.0f};

            // Edge-scroll continues while the cursor rests near the viewport border.
            if (m_mousePos.x <= kEdgeThreshold)          delta.x -= kEdgeScrollSpeed;
            if (m_mousePos.x >= vp.x - kEdgeThreshold)   delta.x += kEdgeScrollSpeed;
            if (m_mousePos.y <= kEdgeThreshold)          delta.y -= kEdgeScrollSpeed;
            if (m_mousePos.y >= vp.y - kEdgeThreshold)   delta.y += kEdgeScrollSpeed;
            if (delta.x != 0.0f || delta.y != 0.0f)
                m_camera.moveBy(delta);
        }

        for (auto it = m_viewModels.begin(); it != m_viewModels.end();) {
            auto &vm = *it;

            if (!vm || vm->expired()) {
                it = m_viewModels.erase(it);
                continue;
            }

            if (vm->visible()) {
                vm->update();
            }

            ++it;
        }
    }

    void GameUIManager::render() {
        m_renderQueue.clear();

        m_renderQueue.emplace(
            core::render::RenderLayer::UI,
            -100,
            core::render::UpdateHudResources {
                m_world.playerResources(core::model::PlayerId::Local)
            }
        );

        core::render::UpdateHudSelection selection {};
        selection.primaryName = "No unit selected";
        selection.action = "None";

        for (const auto& element : m_world.getElements()) {
            auto gameElement = std::dynamic_pointer_cast<core::model::IGameElement>(element);
            if (!gameElement ||
                !gameElement->state().selected ||
                gameElement->getAction() == core::model::ActionType::Dead) {
                continue;
            }

            ++selection.selectedCount;

            // Classify every selected element so the multi-selection portrait strip
            // can show one entry per unit (capped to keep the render command small).
            core::render::HudPortrait portrait;
            if (auto unit = std::dynamic_pointer_cast<core::model::Unit>(element)) {
                portrait.kind = unit->isWorker()
                    ? core::render::HudSelectionKind::Worker
                    : core::render::HudSelectionKind::CombatUnit;
                const float maxHp = unit->getMaxHp();
                portrait.hp01 = maxHp > 0.0f ? unit->getHp() / maxHp : 0.0f;
            } else if (auto building = std::dynamic_pointer_cast<core::model::Building>(element)) {
                portrait.kind = core::render::HudSelectionKind::Building;
                const float maxHp = building->getMaxHp();
                portrait.hp01 = maxHp > 0.0f ? building->getHp() / maxHp : 0.0f;
            } else if (std::dynamic_pointer_cast<core::model::ResourceNode>(element)) {
                portrait.kind = core::render::HudSelectionKind::Resource;
            }
            constexpr std::size_t kMaxPortraits = 24;
            if (selection.portraits.size() < kMaxPortraits) {
                selection.portraits.push_back(portrait);
            }

            if (!selection.hasPrimaryUnit) {
                selection.hasPrimaryUnit = true;
                selection.primaryName = gameElement->displayName();
                selection.action = actionText(gameElement->getAction());

                if (auto unit = std::dynamic_pointer_cast<core::model::Unit>(element)) {
                    selection.hp = unit->getHp();
                    selection.maxHp = unit->getMaxHp();
                    selection.hasCombatStats = true;
                    selection.attackDamage = unit->getAttackDamage();
                    selection.armor = unit->getArmor();
                    selection.attackRange = unit->getAttackRange();
                    selection.kind = unit->isWorker()
                        ? core::render::HudSelectionKind::Worker
                        : core::render::HudSelectionKind::CombatUnit;
                } else if (auto building = std::dynamic_pointer_cast<core::model::Building>(element)) {
                    selection.hp = building->getHp();
                    selection.maxHp = building->getMaxHp();
                    selection.kind = core::render::HudSelectionKind::Building;
                    const auto& bdata = core::data::buildingStaticDataFor(building->buildingType());
                    const bool typeProduces = !bdata.produces.empty();
                    selection.producesUnits = typeProduces;
                    selection.canProduce = building->isComplete() && typeProduces;
                    selection.underConstruction = !building->isComplete();
                    selection.buildProgress01 = building->buildProgress01();
                    selection.trainProgress01 = building->trainProgress();
                    selection.trainQueueCount = building->trainQueueSize();
                    // Resource-affordability lock for the Train button (Epic 6.4).
                    if (typeProduces) {
                        const auto defaultUnit = bdata.produces.front();
                        const auto cost = core::data::unitStaticDataFor(defaultUnit).cost();
                        selection.trainAffordable =
                            m_world.playerResources(building->getTeamId()).canAfford(cost);
                    }
                } else if (auto resource = std::dynamic_pointer_cast<core::model::ResourceNode>(element)) {
                    selection.hp = resource->remaining();
                    selection.maxHp = resource->totalAmount();
                    selection.kind = core::render::HudSelectionKind::Resource;
                }

                selection.position = gameElement->getPosition();
            }
        }

        m_renderQueue.emplace(
            core::render::RenderLayer::UI,
            -99,
            std::move(selection)
        );

        // Build placement preview: a green (placeable) / red (blocked) footprint
        // ghost tracks the cursor while build mode is armed (Epic 0.5.2).
        if (m_worldOrderMode == WorldOrderMode::Build && m_hasMousePos) {
            const auto data = core::data::buildingStaticDataFor(core::model::BuildingType::Barracks);
            const auto& tf = m_world.gridTransform();
            const core::model::Vector2D worldPos = m_camera.screenToWorld(m_mousePos);
            const auto centerCell = tf.worldToGrid(worldPos);
            const int originX = centerCell.x - data.footprintWidth / 2;
            const int originY = centerCell.y - data.footprintHeight / 2;

            // Mirror GameLogicManager::canPlaceBuilding so the color matches the
            // logic layer's accept/reject decision.
            bool placeable = true;
            for (int dy = 0; dy < data.footprintHeight && placeable; ++dy) {
                for (int dx = 0; dx < data.footprintWidth; ++dx) {
                    if (m_world.isTileBlocked(originX + dx, originY + dy) ||
                        m_world.isCellOccupied(originX + dx, originY + dy)) {
                        placeable = false;
                        break;
                    }
                }
            }

            const core::model::Vector2D originCenter =
                tf.gridToWorldCenter(core::path::GridPos{ originX, originY });
            const core::model::Vector2D topLeft {
                originCenter.x - tf.tileSize * 0.5f,
                originCenter.y - tf.tileSize * 0.5f
            };
            const core::model::Vector2D bottomRight {
                topLeft.x + static_cast<float>(data.footprintWidth) * tf.tileSize,
                topLeft.y + static_cast<float>(data.footprintHeight) * tf.tileSize
            };

            m_renderQueue.emplace(
                core::render::RenderLayer::World,
                50,  // above world sprites so the ghost reads clearly
                core::render::DrawRect {
                    .rect = core::model::Rect{ topLeft, bottomRight },
                    .border_color = placeable ? 0xFF44EE44u : 0xFFEE2222u,
                    .color        = placeable ? 0x4044EE44u : 0x40EE2222u
                });
        }

        std::string cursorKey = "default";

        if (m_isDragging) {
            cursorKey = "drag";
        } else if (m_worldOrderMode == WorldOrderMode::Move ||
                   m_worldOrderMode == WorldOrderMode::Patrol ||
                   m_worldOrderMode == WorldOrderMode::Gather) {
            cursorKey = "move";
        } else if (m_worldOrderMode == WorldOrderMode::Attack ||
                   m_worldOrderMode == WorldOrderMode::AttackMove) {
            core::model::Vector2D worldPos = m_camera.screenToWorld(m_mousePos);
            bool hoverEnemy = false;
            for (const auto& element : m_world.getElements()) {
                auto unit = std::dynamic_pointer_cast<core::model::Unit>(element);
                if (unit && unit->getTeamId() != core::model::TeamId::Player && unit->getAction() != core::model::ActionType::Dead) {
                    if (unit->getPosition().distanceTo(worldPos) < 32.0f) {
                        hoverEnemy = true;
                        break;
                    }
                }
            }
            if (hoverEnemy) {
                cursorKey = "attack";
            }
        }

        m_renderQueue.emplace(
            core::render::RenderLayer::UI,
            -98,
            core::render::UpdateHudCursor { cursorKey }
        );

        for (auto &element: m_elements) {
            element->buildRenderCommands(m_renderQueue);
        }

        for (auto &viewModel: m_viewModels) {
            viewModel->buildRenderCommands(m_renderQueue);
        }

        // Victory / defeat banner centered on screen once the match is decided.
        const auto result = m_world.gameResult();
        if (result != core::world::GameResult::InProgress) {
            const auto& vp = m_camera.viewportSize();
            const bool won = result == core::world::GameResult::Victory;
            m_renderQueue.emplace(
                core::render::RenderLayer::UI,
                -50,
                core::render::DrawText {
                    .pos = { vp.x * 0.5f - 120.0f, vp.y * 0.5f - 24.0f },
                    .color = won ? 0x33FF66FFu : 0xFF3333FFu,
                    .fontId = 1u,
                    .size = 48,
                    .text = won ? "VICTORY" : "DEFEAT"
                }
            );
            m_renderQueue.emplace(
                core::render::RenderLayer::UI,
                -50,
                core::render::DrawText {
                    .pos = { vp.x * 0.5f - 120.0f, vp.y * 0.5f + 36.0f },
                    .color = 0xFFFFFFFFu,
                    .fontId = 1u,
                    .size = 20,
                    .text = "Press Enter to restart"
                }
            );
        }
    }

    void GameUIManager::syncWithWorld() {
        // 1. 기존 ViewModel 중 expired 제거
        std::erase_if(m_viewModels,
                      [](const auto &vm) { return vm->expired(); });

        // 2. Logic에 있는데 ViewModel 없는 것 생성
        for (auto element: m_world.getElements()) {
            if (!hasViewModelFor(element)) {
                if (auto unit = std::dynamic_pointer_cast<core::model::Unit>(element)) {
                    m_viewModels.push_back(
                        std::make_shared<core::viewmodel::UnitViewModel>(unit)
                    );
                } else if (auto building = std::dynamic_pointer_cast<core::model::Building>(element)) {
                    m_viewModels.push_back(
                        std::make_shared<core::viewmodel::BuildingViewModel>(building)
                    );
                } else if (auto resource = std::dynamic_pointer_cast<core::model::ResourceNode>(element)) {
                    m_viewModels.push_back(
                        std::make_shared<core::viewmodel::ResourceNodeViewModel>(resource)
                    );
                }
            }
        }

        // Projectiles live outside m_elements; give each a view model too.
        for (const auto& projectile : m_world.projectiles()) {
            const void* target = projectile.get();
            const bool exists = std::any_of(m_viewModels.begin(), m_viewModels.end(),
                [target](const auto& vm) { return vm->modelPtr() == target; });
            if (!exists) {
                m_viewModels.push_back(
                    std::make_shared<core::viewmodel::ProjectileViewModel>(projectile));
            }
        }
    }

    bool GameUIManager::hasViewModelFor(const std::shared_ptr<core::model::IElement> &element) const {
        if (!element)
            return false;

        const void *target = element.get();

        for (const auto &vm: m_viewModels) {
            if (vm->modelPtr() == target) {
                return true;
            }
        }
        return false;
    }
}
