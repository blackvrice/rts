//
// Created by black on 26. 1. 1..
//

#include <game/login/LoginLogicManager.hpp>

namespace rts::core::manager {
    LoginLogicManager::LoginLogicManager(command::LogicCommandBus &bus,
                                         command::LogicCommandRouter &router) : ILogicManager(bus, router) {
    }


    void LoginLogicManager::update() {

    }

    void LoginLogicManager::tick(float dt) {

    }

}
