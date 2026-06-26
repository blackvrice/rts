//
// Created by black on 25. 12. 25..
//

#pragma once

#include "core/manager/ILogicManager.hpp"

namespace rts::core::manager {
    // The lobby has no simulation; it only hosts menu UI. Logic hooks are no-ops.
    class LobbyLogicManager : public ILogicManager {
    public:
        LobbyLogicManager(command::LogicCommandBus&, command::LogicCommandRouter&);
        void update() override;
        void tick(float dt) override;
    };
}
