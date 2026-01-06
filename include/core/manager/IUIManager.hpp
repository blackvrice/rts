//
// Created by black on 25. 12. 22..
//

#pragma once
#include <core/command/CommandRouterBase.hpp>
#include <core/command/LogicCommandBus.hpp>

#include "core/render/RenderQueue.hpp"

namespace rts::manager {
    class IUIManager {
    public:
        IUIManager(
            command::UICommandRouter &uiRouter,
            command::LogicCommandBus &logicBus,
            core::render::RenderQueue &renderQueue
        )
            : m_uiRouter(uiRouter)
              , m_logicBus(logicBus)
              , m_renderQueue(renderQueue) {
        }

        virtual ~IUIManager() = default;

        // 1) UICommand 처리 (UI 입력 / UI 상태 변화)
        //    - uiRouter.dispatch(cmd) 로 UICommand 처리
        //    - 필요하면 m_logicBus.push(LogicCommand) 로 Logic 시스템에 전달
        virtual void update() = 0;

        // 2) 렌더링 명령만 생성하는 함수
        //    - RenderQueue.add(DrawRect / DrawText / DrawSprite ...)
        //    - 실제 그리기는 RenderThread or Window 에서 실행됨
        virtual void render() = 0;

    protected:
        command::UICommandRouter &m_uiRouter; // UI 이벤트 라우팅
        command::LogicCommandBus &m_logicBus; // LogicThread 에게 이벤트 전달용
        core::render::RenderQueue &m_renderQueue; // 화면 그리기 명령 저장
    };
} // namespace rts::manager
