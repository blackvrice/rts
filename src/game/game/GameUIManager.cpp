//
// Created by black on 26. 1. 1..
//

#include "game/game/GameUIManager.hpp"

#include "core/model/Unit.hpp"
#include "core/ui/SelectBox.hpp"
#include "core/ui/TextBox.hpp"
#include "core/viewmodel/UnitViewModel.hpp"
#include "game/game/GameLogicManager.hpp"

namespace rts::core::manager {
    GameUIManager::GameUIManager(command::UICommandRouter &router, command::LogicCommandBus &logicBus,
                                 core::render::RenderQueue &render_queue, world::GameWorld& world) : m_world(world), IUIManager(
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
                const core::model::Vector2D &pos = cmd.position();
                m_logicBus.push(std::make_unique<command::MoveCommand>(pos));
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
        m_elements.push_back(std::make_unique<core::ui::SelectBox>(logicBus));
    }


    void GameUIManager::update() {
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
