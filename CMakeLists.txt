cmake_minimum_required(VERSION 3.22)
project(RTS)

set(CMAKE_CXX_STANDARD 23)

# SFML Root Directory 설정
set(SFML_DIR "D:/Libs/SFML-3.0.2/lib/cmake/SFML")

find_package(SFML 3 REQUIRED COMPONENTS System Window Graphics)

add_executable(RTS src/main.cpp
        include/core/thread/ThreadBase.hpp
        include/core/di/DIContainer.hpp
        include/core/command/Command.hpp
        include/core/command/LogicCommandBus.hpp
        include/core/thread/LogicThread.hpp
        src/core/thread/LogicThread.cpp
        include/core/app/GameApp.hpp
        src/core/di/GameApp.cpp
        include/core/ecs/Entity.hpp
        include/core/ecs/Registry.hpp
        include/core/model/Snapshot.hpp
        include/core/model/GameModel.hpp
        include/core/model/UnitSnapshot.hpp
        include/core/ui/IUIElement.hpp
        include/core/model/TileMap.hpp
        include/core/model/Element.hpp
        include/core/Manager/ILogicManger.hpp
        include/platform/IWindow.hpp
        include/platform/sfml/sfmlWindow.hpp
        src/platform/sfml/sfmlWindow.cpp
        include/core/model/Vector2D.hpp
        include/core/Manager/IUIManager.hpp
        include/core/type/SceneType.hpp
        src/core/app/DIContainer.cpp
        include/core/factory/UnitFactory.hpp
        include/core/prototype/UnitPrototypeLoader.hpp
        include/core/entity/UnitEntity.hpp
        include/core/data/UnitStaticData.hpp
        include/core/data/BuildingStaticData.hpp
        include/core/entity/BuildingEntity.hpp
        include/core/factory/BuildingFactory.hpp
        include/core/data/TileStaticData.hpp
        include/core/map/TileMapInstance.hpp
        include/core/entity/TileInstance.hpp
        include/core/command/UICommandBus.hpp
        include/core/command/UICommand.hpp
        include/core/command/LogicCommand.hpp
        include/core/Manager/SceneManager.hpp
        include/core/scene/IScene.hpp
        include/game/game/GameScene.hpp
        src/game/game/GameScene.cpp
        include/game/game/GameLogicManager.hpp
        include/game/game/GameUIManager.hpp
        include/game/login/LoginScene.hpp
        src/game/login/LoginScene.cpp
        include/game/login/LoginLogicManager.hpp
        include/game/login/LoginUIManager.hpp
        include/game/lobby/LobbyLogicManager.hpp
        include/game/lobby/LobbyUIManager.hpp
        include/game/lobby/LobbyScene.hpp
        src/game/lobby/LobbyScene.cpp
        src/manager/LoginUIManager.cpp
        include/core/ui/TextBox.hpp
        include/core/ui/Signal.hpp
        src/core/ui/TextBox.cpp
        include/core/command/UIRenderCommand.hpp
        include/core/ui/UIRenderType.hpp
        include/core/builder/GameViewModelBuilder.hpp
        include/core/viewmodel/ViewModel.hpp
        include/core/render/RenderQueue.hpp
        include/core/render/RenderCommand.hpp
        include/core/render/IRenderManager.hpp
        include/core/render/RenderManager.hpp
        include/platform/sfml/SfmlRenderManager.hpp
        src/platform/sfml/SfmlRenderManager.cpp
        include/core/render/RenderContext.hpp
        src/platform/sfml/SfmlRenderManager.cpp
        src/platform/sfml/SfmlRenderManager.cpp
        src/platform/sfml/SfmlRenderManager.cpp
)

add_custom_command(TARGET RTS POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "D:/Libs/SFML-3.0.2/bin"
        $<TARGET_FILE_DIR:RTS>
)

target_link_libraries(RTS PRIVATE SFML::System SFML::Window SFML::Graphics)
target_include_directories(RTS PRIVATE include)