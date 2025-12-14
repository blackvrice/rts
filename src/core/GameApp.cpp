//
// Created by black on 25. 12. 6..
//
// src/core/GameApp.cpp
#include "core/GameApp.hpp"
#include "component/RenderInfo.hpp"
#include "scene/SceneManager.hpp"
#include "scene/MenuScene.hpp"
#include "core/LogicThread.hpp"
#include "scene/LoginScene.hpp"
#include "view/RenderLoop.hpp"

namespace rts::core {

    GameApp::GameApp(DIContainer& di)
        : m_di(di)
    {}

    void GameApp::run()
    {
        // 1) GameManager 를 생성 (싱글톤 등록)
        auto game = m_di.registerSingleton<rts::manager::GameManager>();

        // 2) 초기 유닛 구성
        setupInitialEntities(*game);

        // 3) LogicThread 시작
        LogicThread logic(game);
        logic.start();

        // 4) SceneManager 생성
        auto mgr = m_di.registerSingleton<rts::scene::SceneManager>();

        // 5) 첫 화면: 메뉴
        mgr->push(std::make_shared<rts::scene::LoginScene>(mgr, game));

        // 6) 렌더 루프 실행
        rts::view::RenderLoop loop(mgr);
        loop.run();

        // 7) 프로그램 종료 → LogicThread 종료
        logic.stop();
    }

    void GameApp::setupInitialEntities(rts::manager::GameManager& game)
    {
        ecs::Registry& reg = game.registry();

        auto e1 = reg.createEntity();
        reg.add<rts::component::Position>(e1, {{300, 300}});
        reg.add<rts::component::RenderInfo>(e1, {sf::Color::White, 8.f});
        reg.add<rts::component::Selection>(e1, {false});

        auto e2 = reg.createEntity();
        reg.add<rts::component::Position>(e2, {{350, 300}});
        reg.add<rts::component::RenderInfo>(e2, {sf::Color::White, 8.f});
        reg.add<rts::component::Selection>(e2, {false});
    }

} // namespace rts::core
