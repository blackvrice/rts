//
// Created by black on 26. 1. 1..
//

#include "game/game/GameUIManager.hpp"

#include "core/model/Unit.hpp"
#include "core/ui/SelectBox.hpp"
#include "core/ui/TextBox.hpp"
#include "core/viewmodel/UnitViewModel.hpp"
#include "game/game/GameLogicManager.hpp"

namespace rts::manager {
    GameUIManager::GameUIManager(command::UICommandRouter &router, command::LogicCommandBus &logicBus,
                                 core::render::RenderQueue &render_queue) : manager::IUIManager(
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

    void GameUIManager::addViewModel(std::shared_ptr<core::viewmodel::IViewModel> vm) {
        m_viewModels.push_back(vm);
    }

    void GameUIManager::render() {
        m_renderQueue.clear();
        for (auto &element: m_elements) {
            element->buildRenderCommands(m_renderQueue);
        }
    }

    void GameUIManager::setLogicSource(ILogicManager& logic) {
        m_logic = &logic;
    }


    void GameUIManager::syncWithLogic() {
        if (!m_logic)
            return;

        // 1. 기존 ViewModel 중 expired 제거
        std::erase_if(m_viewModels,
                      [](const auto &vm) { return vm->expired(); });

        // 2. Logic에 있는데 ViewModel 없는 것 생성
        for (auto &element: *m_logic->) {
            if (!hasViewModelFor(element)) {
                if (auto unit = std::dynamic_pointer_cast<core::model::Unit>(element)) {
                    m_viewModels.push_back(
                        std::make_shared<core::viewmodel::UnitViewModel>(unit)
                    );
                }
            }
        }
    }

    bool GameUIManager::hasViewModelFor(
        const std::shared_ptr<core::model::IGameElement> &element) const {
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
