//
// Created by black on 25. 12. 25..
//

#pragma once

#include <core/Manager/IUIManager.hpp>

#include "core/manager/ILogicManager.hpp"

namespace rts::core::manager {
    class LobbyLogicManager : public ILogicManager {
        LobbyLogicManager(command::LogicCommandBus&, command::LogicCommandRouter&);
        void update() override;
    };
}
