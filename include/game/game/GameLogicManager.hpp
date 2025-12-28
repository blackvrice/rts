//
// Created by black on 25. 12. 25..
//

#pragma once

#include <core/command/CommandRouterBase.hpp>
#include <core/command/LogicCommandBus.hpp>

namespace rts::manager {
    class GameLogicManager : public ILogicManager {
    public:
        GameLogicManager(command::LogicCommandBus&, command::LogicCommandRouter&);
        void start() override;
        void update() override;
    };
}
