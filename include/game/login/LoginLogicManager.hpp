//
// Created by black on 25. 12. 25..
//

#pragma once

#include <core/Manager/IUIManager.hpp>
#include <core/command/CommandRouterBase.hpp>

namespace rts::manager {
    class LoginLogicManager : public ILogicManager {
    public:
        LoginLogicManager(command::LogicCommandBus&, command::LogicCommandRouter&);
        void start() override;
        void update() override;
    };
}
