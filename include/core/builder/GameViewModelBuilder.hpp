//
// Created by black on 25. 12. 26..
//

#pragma once

#include "logic/GameLogicManager.hpp"
#include "viewmodel/GameViewModel.hpp"

namespace rts::presenter {

    class GameViewModelBuilder {
    public:
        rts::viewmodel::GameViewModel build(
            const rts::logic::GameLogicManager& logic
        ) const;
    };

}