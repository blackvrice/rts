//
// Created by black on 25. 12. 25..
//

#pragma once

#include <core/Manager/IUIManager.hpp>
#include "core/model/Vector2D.hpp"

#include <chrono>
#include <memory>
#include <vector>

namespace rts::core::model {
    class IElement;
}

namespace rts::core::command {
    class GameplayInputCommand;
}

namespace rts::core::ui {
    class IUIElement;
}

namespace rts::core::viewmodel {
    class IViewModel;
}

namespace rts::core::world {
    class GameWorld;
}

namespace rts::core::manager {
    class CameraManager;

    class GameUIManager : public IUIManager {
    public:
        GameUIManager(command::UICommandRouter &router, command::LogicCommandBus &logicBus,
                      core::render::RenderQueue &render_queue, core::world::GameWorld &world,
                      CameraManager &camera);
        ~GameUIManager() override;

        void update() override;

        bool hasViewModelFor(const std::shared_ptr<core::model::IElement>& element) const;

        void render() override;

        void syncWithWorld() override;
    private:
        enum class WorldOrderMode {
            Move,
            Attack,
            AttackMove,
            Patrol,
            Gather,
            Build
        };

        void issueWorldOrder(const core::model::Vector2D& screenPosition);
        // Same as issueWorldOrder but takes a world-space point directly (minimap).
        void issueWorldOrderAtWorld(const core::model::Vector2D& worldPos);
        void handleGameplayInput(const command::GameplayInputCommand& cmd);

        std::vector<int> unitElement;
        std::vector<std::unique_ptr<core::ui::IUIElement>> m_elements;
        std::vector<std::shared_ptr<core::viewmodel::IViewModel>> m_viewModels;
        core::world::GameWorld& m_world;
        CameraManager& m_camera;
        WorldOrderMode m_worldOrderMode { WorldOrderMode::Attack };
        bool m_shift { false };
        bool m_ctrl { false };
        // Select-all-of-type request for the next release (ctrl held or double-click).
        bool m_selectSameType { false };
        std::chrono::steady_clock::time_point m_lastClickTime {};
        core::model::Vector2D m_lastClickPos {};
        core::model::Vector2D m_mousePos {};
        bool m_hasMousePos { false };
        bool m_isDragging { false };
        bool m_showDebugOverlay { false };  // F3 toggles tick/world-hash readout
    };
}
