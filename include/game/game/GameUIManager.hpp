//
// Created by black on 25. 12. 25..
//

#pragma once

#include <../core/Manager/IUIManager.hpp>

#include "command/CommandRouterBase.hpp"

namespace rts::manager {
    class GameUIManager : public IUIManager {
    public:
        GameUIManager(command::UICommandRouter& command_bus);

        void update() override;
        void render() override;
    };
}
