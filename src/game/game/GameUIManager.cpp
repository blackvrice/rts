//
// Created by black on 26. 1. 1..
//

#include "game/game/GameUIManager.hpp"

#include "core/command/CommandRouterBase.hpp"
#include "core/command/LogicCommand.hpp"
#include "core/command/LogicCommandBus.hpp"
#include "core/command/UICommand.hpp"
#include "core/manager/CameraManager.hpp"
#include "core/model/IElement.hpp"
#include "core/model/IGameElement.hpp"
#include "core/model/Unit.hpp"
#include "core/render/RenderQueue.hpp"
#include "core/ui/IUIElement.hpp"
#include "core/ui/SelectBox.hpp"
#include "core/ui/TextBox.hpp"
#include "core/model/IViewModel.hpp"
#include "core/viewmodel/UnitViewModel.hpp"
#include "core/world/GameWorld.hpp"
#include "game/game/GameLogicManager.hpp"

namespace rts::core::manager {
    namespace {
        constexpr float kCameraStep = 128.0f;
        constexpr float kEdgeThreshold = 20.0f;
        constexpr float kEdgeScrollSpeed = 8.0f;
    }

    GameUIManager::~GameUIManager() = default;

    GameUIManager::GameUIManager(command::UICommandRouter &router, command::LogicCommandBus &logicBus,
                                 core::render::RenderQueue &render_queue, world::GameWorld& world,
                                 CameraManager& camera) : m_world(world), m_camera(camera), IUIManager(
        router, logicBus, render_queue) {

        router.on<command::MouseLeftPressedCommand>(
            [this](const command::MouseLeftPressedCommand &cmd) {
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
                const core::model::Vector2D &pos = cmd.position();
                for (const auto &element: m_elements) {
                    element->MouseUp(pos);
                }
            }
        );

        router.on<command::MouseRightPressedCommand>(
            [this](const command::MouseRightPressedCommand &cmd) {
                const core::model::Vector2D worldPos = m_camera.screenToWorld(cmd.position());
                m_logicBus.push(std::make_unique<command::MoveCommand>(worldPos));
            }
        );

        router.on<command::KeyPressedCommand>(
            [this](const command::KeyPressedCommand &cmd) {
                const core::model::Key &key = cmd.getCode();
                using KM = core::model::KeyModifier;
                const KM &modifier = cmd.getMods();
                if (static_cast<bool>(modifier & KM::Ctrl)) m_ctrl = false;
                if (static_cast<bool>(modifier & KM::Shift)) m_shift = false;
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
