cmake_minimum_required(VERSION 3.22)
project(RTS)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_GENERATOR MATCHES "Ninja")
        # GCC 16 UCRT debug builds can fail with temp-file/ICE errors under high CLion -j values.
        set_property(GLOBAL PROPERTY JOB_POOLS rts_compile_pool=4)
        set(CMAKE_JOB_POOL_COMPILE rts_compile_pool)
endif()

# SFML Root Directory 설정
set(SFML_DIR "D:/Libs/SFML-3.0.2/lib/cmake/SFML")
set(RTS_TINY_SWORDS_ROOT "${CMAKE_SOURCE_DIR}/Tiny Swords (Free Pack)/Tiny Swords (Free Pack)")
# Data files (JSON) consumed at runtime by DataRegistry. Resolved via a
# configured absolute path so the executable finds them regardless of CWD.
set(RTS_DATA_ROOT "${CMAKE_SOURCE_DIR}/data")

find_package(SFML 3 REQUIRED COMPONENTS System Window Graphics)
find_package(OpenGL REQUIRED)

configure_file(
        "${CMAKE_SOURCE_DIR}/include/platform/sfml/SfmlAssetPaths.hpp.in"
        "${CMAKE_BINARY_DIR}/generated/platform/sfml/SfmlAssetPaths.hpp"
        @ONLY
)

configure_file(
        "${CMAKE_SOURCE_DIR}/include/core/data/DataPaths.hpp.in"
        "${CMAKE_BINARY_DIR}/generated/core/data/DataPaths.hpp"
        @ONLY
)

include(CheckCXXSourceCompiles)

set(RTS_CRT_HEADER_TEST "#include <cstdlib>\n#include <ctime>\nint main(){return 0;}")
check_cxx_source_compiles("${RTS_CRT_HEADER_TEST}" RTS_CRT_HEADERS_COMPILE)

if(MINGW AND NOT RTS_CRT_HEADERS_COMPILE)
        set(RTS_PREVIOUS_CMAKE_REQUIRED_DEFINITIONS "${CMAKE_REQUIRED_DEFINITIONS}")
        set(CMAKE_REQUIRED_DEFINITIONS "-D_UCRT")

        check_cxx_source_compiles("${RTS_CRT_HEADER_TEST}" RTS_CRT_HEADERS_COMPILE_WITH_UCRT)

        set(CMAKE_REQUIRED_DEFINITIONS "${RTS_PREVIOUS_CMAKE_REQUIRED_DEFINITIONS}")

        if(RTS_CRT_HEADERS_COMPILE_WITH_UCRT)
                add_compile_definitions(_UCRT)
        else()
                message(FATAL_ERROR "The MinGW CRT headers do not compile with or without _UCRT.")
        endif()
endif()

# =========================
# ImGui 설정
# =========================
set(IMGUI_DIR "D:/Libs/imgui")

add_library(imgui STATIC
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
        ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
)

target_include_directories(imgui PUBLIC
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
)

if(WIN32)
        target_link_libraries(imgui PUBLIC gdi32 dwmapi)
endif()

# =========================
# RTS 실행 파일
# =========================
add_executable(RTS
        src/main.cpp
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
        include/core/ecs/EntityId.hpp
        include/core/ecs/EntityManager.hpp
        include/core/sim/SimClock.hpp
        include/core/sim/Fixed.hpp
        src/core/sim/Fixed.cpp
        include/core/ui/IUIElement.hpp
        include/core/manager/ILogicManager.hpp
        include/platform/IWindow.hpp
        include/platform/sfml/sfmlWindow.hpp
        src/platform/sfml/SfmlWindow.cpp
        include/core/model/Vector2D.hpp
        include/core/Manager/IUIManager.hpp
        include/core/type/SceneType.hpp
        include/core/factory/UnitFactory.hpp
        include/core/prototype/UnitPrototypeLoader.hpp
        include/core/entity/UnitEntity.hpp
        include/core/data/UnitStaticData.hpp
        include/core/data/BuildingStaticData.hpp
        include/core/data/ResourceStaticData.hpp
        include/core/data/SpriteData.hpp
        include/core/data/DataRegistry.hpp
        include/core/data/DataPaths.hpp.in
        src/core/data/DataRegistry.cpp
        include/core/entity/BuildingEntity.hpp
        include/core/factory/BuildingFactory.hpp
        include/core/data/TileStaticData.hpp
        include/core/entity/TileInstance.hpp
        include/core/command/UICommandBus.hpp
        include/core/command/UICommand.hpp
        include/core/command/LogicCommand.hpp
        include/core/manager/CameraManager.hpp
        src/core/manager/CameraManager.cpp
        include/core/Manager/SceneManager.hpp
        src/core/manager/SceneManager.cpp
        include/core/scene/IScene.hpp
        include/game/game/GameScene.hpp
        src/game/game/GameScene.cpp
        include/game/game/GameLogicManager.hpp
        include/game/game/systems/CollisionSystem.hpp
        src/game/game/systems/CollisionSystem.cpp
        include/game/game/systems/ControlGroupSystem.hpp
        src/game/game/systems/ControlGroupSystem.cpp
        include/game/game/systems/MovementSystem.hpp
        src/game/game/systems/MovementSystem.cpp
        include/game/game/systems/SelectionSystem.hpp
        src/game/game/systems/SelectionSystem.cpp
        include/game/game/GameUIManager.hpp
        include/game/login/LoginScene.hpp
        src/game/login/LoginScene.cpp
        include/game/login/LoginLogicManager.hpp
        include/game/login/LoginUIManager.hpp
        include/game/lobby/LobbyLogicManager.hpp
        include/game/lobby/LobbyUIManager.hpp
        include/game/lobby/LobbyScene.hpp
        src/game/lobby/LobbyScene.cpp
        src/game/login/LoginUIManager.cpp
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
        include/platform/sfml/SfmlHudOverlay.hpp
        src/platform/sfml/SfmlHudOverlay.cpp
        include/platform/sfml/SfmlAssetPaths.hpp.in
        include/core/render/RenderContext.hpp
        src/game/login/LoginLogicManager.cpp
        src/game/lobby/LobbyLogicManager.cpp
        src/game/lobby/LobbyUIManager.cpp
        src/game/game/GameUIManager.cpp
        src/game/game/GameLogicManager.cpp
        include/core/factory/SceneFacotry.hpp
        include/core/model/Key.hpp
        include/core/model/Event.hpp
        include/core/font/FontMetrics.hpp
        include/platform/sfml/SfmlFontMetrics.hpp
        include/core/font/FontTypes.hpp
        include/core/font/FontManager.hpp
        src/core/font/FontManager.cpp
        include/core/model/IElement.hpp
        include/core/model/IGameElement.hpp
        include/core/model/IUIElement.hpp
        include/core/model/Unit.hpp
        include/core/model/PlayerResourceState.hpp
        include/core/model/State.hpp
        include/core/model/Data.hpp
        include/core/model/Entity.hpp
        include/core/ui/SelectBox.hpp
        src/core/ui/SelectBox.cpp
        include/core/model/Rect.hpp
        include/core/model/IViewModel.hpp
        include/core/model/UnitPanelViewModel.hpp
        include/core/viewmodel/UnitViewModel.hpp
        src/core/model/Unit.cpp
        src/core/viewmodel/UnitViewModel.cpp
        include/core/map/TileMapSoA.hpp
        include/core/world/GameWorld.hpp
        include/core/manager/PathManager.hpp
        src/core/manager/PathManager.cpp
        include/core/path/GridTypes.hpp
        include/core/path/IGridQuery.hpp
        include/core/path/Path.hpp
        include/core/world/GameWorldGridQuery.hpp
        src/core/world/GameWorld.cpp
        src/core/world/GameWorldGridQuery.cpp
        include/core/world/GridTransform.hpp
        src/core/model/Building.cpp
        src/core/map/FogOfWar.cpp
        src/core/model/Projectile.cpp
        src/core/model/ResourceNode.cpp
        src/core/viewmodel/BuildingViewModel.cpp
        src/core/viewmodel/ProjectileViewModel.cpp
        src/core/viewmodel/ResourceNodeViewModel.cpp
        include/core/map/FogOfWar.hpp
        include/core/model/Building.hpp
        include/core/model/Projectile.hpp
        include/core/model/ResourceNode.hpp
        include/core/model/UnitType.hpp
        include/core/viewmodel/BuildingViewModel.hpp
        include/core/viewmodel/ProjectileViewModel.hpp
        include/core/viewmodel/ResourceNodeViewModel.hpp
)

# tmxlite
add_subdirectory(external/tmxlite/tmxlite)

if(MINGW AND TARGET tmxlite)
        target_compile_definitions(tmxlite PRIVATE __NO_MINGW_LFS)
endif()

target_include_directories(RTS PRIVATE
        include
        "${CMAKE_BINARY_DIR}/generated"
        external/json
)

target_link_libraries(RTS PRIVATE
        SFML::System
        SFML::Window
        SFML::Graphics
        OpenGL::GL
        tmxlite
        imgui
)

if(MINGW)
        get_filename_component(RTS_COMPILER_RUNTIME_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)

        if(RTS_CRT_HEADERS_COMPILE_WITH_UCRT AND EXISTS "C:/msys64/ucrt64/bin")
                set(RTS_MINGW_RUNTIME_DIR "C:/msys64/ucrt64/bin")
        else()
                set(RTS_MINGW_RUNTIME_DIR "${RTS_COMPILER_RUNTIME_DIR}")
        endif()
endif()

add_custom_command(TARGET RTS POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "D:/Libs/SFML-3.0.2/bin"
        $<TARGET_FILE_DIR:RTS>
)

if(MINGW)
        foreach(RTS_RUNTIME_DLL libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll)
                if(EXISTS "${RTS_MINGW_RUNTIME_DIR}/${RTS_RUNTIME_DLL}")
                        add_custom_command(TARGET RTS POST_BUILD
                                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                "${RTS_MINGW_RUNTIME_DIR}/${RTS_RUNTIME_DLL}"
                                $<TARGET_FILE_DIR:RTS>
                        )
                endif()
        endforeach()
endif()
