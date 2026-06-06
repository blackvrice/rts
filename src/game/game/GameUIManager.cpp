//
// Created by black on 26. 1. 1..
//

#include "game/game/GameUIManager.hpp"

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
#include "core/render/RenderCommand.hpp"
#include "core/render/RenderQueue.hpp"
#include "core/ui/IUIElement.hpp"
#include "core/ui/SelectBox.hpp"
#include "core/ui/TextBox.hpp"
#include "core/model/IViewModel.hpp"
#include "core/viewmodel/UnitViewModel.hpp"
#include "core/viewmodel/BuildingViewModel.hpp"
#include "core/viewmodel/ResourceNodeViewModel.hpp"
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
                    action = GameplayInputAction::Attack;
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
                case Key::Escape:
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
                if (static_cast<bool>(modifier & KM::Ctrl)) m_ctrl = false;
                if (static_cast<bool>(modifier & KM::Shift)) m_shift = false;

                command::GameplayInputAction hotkeyAction {};
                // StarCraft-style command hotkeys mirror the HUD command buttons
                // so both input paths stay aligned with the same LogicCommand bridge.
                if (!usesCommandModifier(modifier) && starCraftHotkeyAction(key, hotkeyAction)) {
                    const command::GameplayInputCommand hotkeyCommand(hotkeyAction);
                    handleGameplayInput(hotkeyCommand);
                    return;
                }

                switch (key) {
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
            using KM = core::model::KeyModifier;
            const KM &modifier = cmd.getMods();
            if (static_cast<bool>(modifier & KM::Ctrl)) m_ctrl = false;
            if (static_cast<bool>(modifier & KM::Shift)) m_shift = false;
        });
        m_elements.push_back(std::make_unique<core::ui::SelectBox>(logicBus, m_camera));
    }

    void GameUIManager::issueWorldOrder(const core::model::Vector2D& screenPosition) {
        const core::model::Vector2D worldPos = m_camera.screenToWorld(screenPosition);

        switch (m_worldOrderMode) {
            case WorldOrderMode::Move:
                m_logicBus.push(std::make_unique<command::MoveCommand>(worldPos));
                break;
            case WorldOrderMode::Attack:
                m_logicBus.push(std::make_unique<command::AttackCommand>(worldPos));
                break;
            case WorldOrderMode::Gather:
                m_logicBus.push(std::make_unique<command::GatherCommand>(worldPos));
                break;
        }

        m_worldOrderMode = WorldOrderMode::Attack;
    }

    void GameUIManager::handleGameplayInput(const command::GameplayInputCommand& cmd) {
        switch (cmd.action()) {
            case command::GameplayInputAction::Move:
                m_worldOrderMode = WorldOrderMode::Move;
                break;
            case command::GameplayInputAction::Attack:
            case command::GameplayInputAction::AttackMove:
                m_worldOrderMode = WorldOrderMode::Attack;
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
            case command::GameplayInputAction::Patrol:
            case command::GameplayInputAction::TrainUnit:
            case command::GameplayInputAction::Build:
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
                } else if (auto building = std::dynamic_pointer_cast<core::model::Building>(element)) {
                    selection.hp = building->getHp();
                    selection.maxHp = building->getMaxHp();
                } else if (auto resource = std::dynamic_pointer_cast<core::model::ResourceNode>(element)) {
                    selection.hp = resource->remaining();
                    selection.maxHp = resource->remaining(); 
                }

                selection.position = gameElement->getPosition();
            }
        }

        m_renderQueue.emplace(
            core::render::RenderLayer::UI,
            -99,
            std::move(selection)
        );

        int cursorId = 100; // Cursor_01

        if (m_isDragging) {
            cursorId = 102; // Cursor_03
        } else if (m_worldOrderMode == WorldOrderMode::Move ||
                   m_worldOrderMode == WorldOrderMode::Gather) {
            cursorId = 103; // Cursor_04
        } else if (m_worldOrderMode == WorldOrderMode::Attack) {
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
                cursorId = 101; // Cursor_02
            }
        }

        m_renderQueue.emplace(
            core::render::RenderLayer::UI,
            -98,
            core::render::UpdateHudCursor { cursorId }
        );

        for (auto &element: m_elements) {
            element->buildRenderCommands(m_renderQueue);
        }

        for (auto &viewModel: m_viewModels) {
            viewModel->buildRenderCommands(m_renderQueue);
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
