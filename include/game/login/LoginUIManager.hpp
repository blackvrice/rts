//
// Created by black on 25. 12. 25..
//

#pragma once

#include <memory>
#include <core/Manager/IUIManager.hpp>


namespace rts::manager {
    class LoginUIManager : public IUIManager {
    public:
        explicit LoginUIManager(command::UICommandRouter&, command::LogicCommandBus&);

        void update() override;
        void render() override;

    private:
        std::vector<std::unique_ptr<ui::IUIElement>> m_elements;
    };
}
