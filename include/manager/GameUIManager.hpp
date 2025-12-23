//
// Created by black on 25. 12. 7..
//
#pragma once
#include <memory>
#include <manager/IManager.hpp>
#include <ui/UIElement.hpp>
#include <command/CommandBus.hpp>
#include <command/CommandRouter.hpp>

#include "IUIManager.hpp"

namespace rts::manager {
    class GameUIManager : public IUIManager {
    public:
        GameUIManager() = default;

        void update() override;

        void render() override;

    private:
        std::vector<std::shared_ptr<ui::UIElement> > m_elements;
    };
}
