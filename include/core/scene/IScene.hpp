//
// Created by black on 25. 12. 25..
//

#pragma once
#include <memory>
#include <typeindex>
#include <core/manager/ILogicManger.hpp>
#include <core/manager/IUIManager.hpp>

namespace rts::scene {
    class IScene {
    public:
        IScene(const std::shared_ptr<manager::IUIManager> &uiManager, const std::shared_ptr<manager::ILogicManager> &logicManager) : m_uiManager(uiManager), m_logicManager(logicManager) {}
        virtual ~IScene() = default;
        virtual void update() = 0;
        virtual void render() = 0;
        manager::IUIManager& getUIManager() const { return *m_uiManager; }
        manager::ILogicManager& getLogicManager() const { return *m_logicManager; }

    protected:
        std::shared_ptr<manager::IUIManager> m_uiManager;
        std::shared_ptr<manager::ILogicManager> m_logicManager;
    };
}
