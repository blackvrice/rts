//
// Created by black on 25. 12. 25..
//

#pragma once

#include <core/Manager/IUIManager.hpp>
#include <core/command/CommandRouterBase.hpp>

#include "core/manager/ILogicManager.hpp"

namespace rts::core::manager {
    class LoginLogicManager : public ILogicManager {
    public:
        LoginLogicManager(command::LogicCommandBus&, command::LogicCommandRouter&);
        void update() override;
        void tick(float dt) override;
    };
}
