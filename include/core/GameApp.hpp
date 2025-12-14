//
// Created by black on 25. 12. 6..
//

#pragma once
#include "core/DIContainer.hpp"
#include "manager/GameManager.hpp"

namespace rts::core {

    class GameApp {
    public:
        explicit GameApp(DIContainer& di);

        void run();

    private:
        void setupInitialEntities(rts::manager::GameManager& game);

        DIContainer& m_di;
    };

} // namespace rts::core
