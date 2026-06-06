//
// Created by black on 25. 12. 25..
//

#pragma once

#include <core/Manager/IUIManager.hpp>
#include "core/model/Vector2D.hpp"

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
            Attack
        };

        void issueWorldOrder(const core::model::Vector2D& screenPosition);
        void handleGameplayInput(const command::GameplayInputCommand& cmd);

        std::vector<int> unitElement;
        std::vector<std::unique_ptr<core::ui::IUIElement>> m_elements;
        std::vector<std::shared_ptr<core::viewmodel::IViewModel>> m_viewModels;
        core::world::GameWorld& m_world;
        CameraManager& m_camera;
        WorldOrderMode m_worldOrderMode { WorldOrderMode::Attack };
        bool m_shift { false };
        bool m_ctrl { false };
        core::model::Vector2D m_mousePos {};
        bool m_hasMousePos { false };
        bool m_isDragging { false };
    };
}
