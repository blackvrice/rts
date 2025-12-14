cmake_minimum_required(VERSION 3.22)
project(RTS)

set(CMAKE_CXX_STANDARD 23)

# SFML Root Directory 설정
set(SFML_DIR "D:/Libs/SFML-3.0.2/lib/cmake/SFML")

find_package(SFML 3 REQUIRED COMPONENTS System Window Graphics)

add_executable(RTS src/main.cpp
        include/core/ThreadBase.hpp
        include/core/DIContainer.hpp
        include/core/Command.hpp
        include/core/CommandBus.hpp
        include/core/LogicThread.hpp
        src/core/LogicThread.cpp
        include/core/GameApp.hpp
        src/core/GameApp.cpp
        include/ecs/Entity.hpp
        include/ecs/Registry.hpp
        include/component/Position.hpp
        include/component/Velocity.hpp
        include/component/Selection.hpp
        include/component/RenderInfo.hpp
        include/model/Snapshot.hpp
        include/model/GameModel.hpp
        src/model/GameModel.cpp
        include/system/MovementSystem.hpp
        src/system/MovementSystem.cpp
        include/presenter/GamePresenter.hpp
        include/presenter/SelectCommand.hpp
        src/presenter/SelectCommand.cpp
        src/presenter/GamePresenter.cpp
        include/view/GameView.hpp
        src/view/GameView.cpp
        include/view/RenderLoop.hpp
        src/view/RenderLoop.cpp
        include/scene/Scene.hpp
        include/scene/SceneManager.hpp
        src/scene/SceneManager.cpp
        include/scene/InGameScene.hpp
        src/scene/InGameScene.cpp
        include/scene/MenuScene.hpp
        src/scene/MenuScene.cpp
        include/manager/GameManager.hpp
        include/manager/UnitManager.hpp
        src/manager/UnitManager.cpp
        include/manager/SystemManager.hpp
        src/manager/SystemManager.cpp
        src/manager/GameManager.cpp
        include/system/SelectionSystem.hpp
        include/presenter/MoveCommand.hpp
        src/system/SelectionSystem.cpp
        include/model/UnitSnapshot.hpp
        include/manager/UIManager.hpp
        include/ui/UIElement.hpp
        include/ui/Button.hpp
        include/ui/TextBox.hpp
        include/scene/LoginScene.hpp
        src/scene/LoginScene.cpp
        include/ui/PasswordTextBox.hpp
        src/ui/PasswordTextBox.cpp
        src/ui/Button.cpp
        include/manager/MapManager.hpp
        include/model/TileMap.hpp
        include/model/Element.hpp
        "include/system/RendererSystem.hpp"
        include/component/TransformComponent.hpp
        include/component/SpriteComponent.hpp
        include/component/AnimationComponent.hpp
        src/system/RendererSystem.cpp
        include/scene/GameSceneBase.hpp
        src/ui/TextBox.cpp
)

add_custom_command(TARGET RTS POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "D:/Libs/SFML-3.0.2/bin"
        $<TARGET_FILE_DIR:RTS>
)

target_link_libraries(RTS PRIVATE SFML::System SFML::Window SFML::Graphics)
target_include_directories(RTS PRIVATE include)