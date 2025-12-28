//
// Created by black on 25. 12. 26..
//

#pragma once

namespace rts::viewmodel {
    struct LoginViewModel {
        bool isLoggingIn;
        std::string errorMessage;
    };

    struct GameHudViewModel {
        int gold;
        int supply;
    };
}