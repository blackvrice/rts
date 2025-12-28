//
// Created by black on 25. 12. 22..
//

#pragma once
#include <core/command/CommandRouterBase.hpp>
#include <core/command/LogicCommandBus.hpp>

namespace rts::manager {
    class IUIManager {
    public:
        IUIManager(command::UICommandRouter& router, command::LogicCommandBus& logicBus) :m_router(router), m_logicBus(logicBus) {}

        virtual ~IUIManager() = default;

        // UICommand 처리 (입력, 상태 변경 등)
        virtual void update() = 0;

        // 화면에 그리기만 담당 (side-effect 없음)
        virtual void render() = 0;
    protected:
        command::UICommandRouter& m_router;
        command::LogicCommandBus& m_logicBus;
    };

}
