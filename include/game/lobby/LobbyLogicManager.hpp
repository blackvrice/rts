//
// Created by black on 25. 12. 25..
//

#pragma once

#include <core/Manager/IUIManager.hpp>

#include "core/manager/ILogicManger.hpp"

namespace rts::manager {
    class LobbyLogicManager : public ILogicManager {
        LobbyLogicManager(command::LogicCommandBus&, command::LogicCommandRouter&);
        void start() override;
        void update() override;
    };
}
