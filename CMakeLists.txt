cmake_minimum_required(VERSION 3.24)
project(forge VERSION 0.9.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SDL3 3.2 REQUIRED CONFIG)
find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
include(FetchContent)
include(CTest)

option(FORGE_DEV_UI "Build the developer overlay" ON)
option(FORGE_GPU_TESTS "Enable desktop GPU smoke tests in CTest" OFF)
# Keep the engine and third-party GPU codecs in one binary. KTX would otherwise
# flip this on and turn camera/assets into extra dylibs.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Forge links statically" FORCE)

FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 8d1fd52e5ab5590e2c81768ace50c72bae28f2ed
    GIT_PROGRESS TRUE
)
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glm)

set(KTX_FEATURE_TESTS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_TOOLS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_GL_UPLOAD OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_VK_UPLOAD OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_LOADTEST_APPS OFF CACHE STRING "" FORCE)
set(KTX_FEATURE_STATIC_LIBRARY ON CACHE BOOL "" FORCE)
FetchContent_Declare(ktx_source
    URL https://codeload.github.com/KhronosGroup/KTX-Software/tar.gz/4d6fc70eaf62ad0558e63e8d97eb9766118327a6
    URL_HASH SHA256=4d0a3c4470c67e0f1544d2a92f379dc919c9627a5c3fa5c4fcaf4c22324827f5
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(ktx_source)

set(TARGET_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(TARGET_HELLO_WORLD OFF CACHE BOOL "" FORCE)
set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "" FORCE)
set(TARGET_SAMPLES OFF CACHE BOOL "" FORCE)
set(TARGET_VIEWER OFF CACHE BOOL "" FORCE)
set(ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(CPP_EXCEPTIONS_ENABLED ON CACHE BOOL "" FORCE)
set(CPP_RTTI_ENABLED ON CACHE BOOL "" FORCE)
FetchContent_Declare(JoltPhysics
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG v5.6.0
    SOURCE_SUBDIR Build
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JoltPhysics)

FetchContent_Declare(lua
    GIT_REPOSITORY https://github.com/lua/lua.git
    GIT_TAG v5.4.7
    GIT_SHALLOW TRUE
)
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()
FetchContent_GetProperties(lua)
if(NOT lua_POPULATED)
    FetchContent_Populate(lua)
endif()
set(LUA_CORE
    ${lua_SOURCE_DIR}/lapi.c ${lua_SOURCE_DIR}/lcode.c ${lua_SOURCE_DIR}/lctype.c ${lua_SOURCE_DIR}/ldebug.c
    ${lua_SOURCE_DIR}/ldo.c ${lua_SOURCE_DIR}/ldump.c ${lua_SOURCE_DIR}/lfunc.c ${lua_SOURCE_DIR}/lgc.c
    ${lua_SOURCE_DIR}/llex.c ${lua_SOURCE_DIR}/lmem.c ${lua_SOURCE_DIR}/lobject.c ${lua_SOURCE_DIR}/lopcodes.c
    ${lua_SOURCE_DIR}/lparser.c ${lua_SOURCE_DIR}/lstate.c ${lua_SOURCE_DIR}/lstring.c ${lua_SOURCE_DIR}/ltable.c
    ${lua_SOURCE_DIR}/ltm.c ${lua_SOURCE_DIR}/lundump.c ${lua_SOURCE_DIR}/lvm.c ${lua_SOURCE_DIR}/lzio.c
    ${lua_SOURCE_DIR}/lauxlib.c ${lua_SOURCE_DIR}/lbaselib.c ${lua_SOURCE_DIR}/lcorolib.c
    ${lua_SOURCE_DIR}/lmathlib.c ${lua_SOURCE_DIR}/lstrlib.c ${lua_SOURCE_DIR}/ltablib.c ${lua_SOURCE_DIR}/lutf8lib.c
)
add_library(lua_static STATIC ${LUA_CORE})
target_include_directories(lua_static PUBLIC ${lua_SOURCE_DIR})
if(APPLE)
    target_compile_definitions(lua_static PRIVATE LUA_USE_MACOSX)
elseif(WIN32)
    target_compile_definitions(lua_static PRIVATE LUA_USE_WINDOWS)
else()
    target_compile_definitions(lua_static PRIVATE LUA_USE_POSIX)
endif()
if(NOT MSVC)
    target_compile_options(lua_static PRIVATE -w)
endif()

add_library(forge_asset_deps STATIC src/engine/assets/gltf/dependencies.cpp third_party/mikktspace/mikktspace.c)
target_include_directories(forge_asset_deps SYSTEM PUBLIC third_party/cgltf third_party/stb third_party/mikktspace)

add_library(forge_assets STATIC
    src/engine/assets/gltf/gltf.cpp
    src/engine/assets/gltf/cube.cpp
    src/engine/assets/gltf/terrain.cpp
    src/engine/assets/ktx2/ktx2.cpp
    src/engine/assets/manager/manager.cpp
)
target_include_directories(forge_assets PUBLIC src)
target_link_libraries(forge_assets PUBLIC glm::glm ktx_read PRIVATE forge_asset_deps)

add_library(forge_anim STATIC src/engine/anim/skeleton/animator.cpp src/engine/anim/locomotion/locomotion.cpp)
target_include_directories(forge_anim PUBLIC src)
target_link_libraries(forge_anim PUBLIC forge_assets glm::glm)

add_library(forge_camera STATIC src/engine/core/camera/camera.cpp)
target_include_directories(forge_camera PUBLIC src)
target_link_libraries(forge_camera PUBLIC glm::glm)

FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG f1cc2ae15e53a861a874c3034aae6798fde194ab
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(imgui)
add_library(forge_imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlgpu3.cpp
)
target_include_directories(forge_imgui SYSTEM PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(forge_imgui PUBLIC SDL3::SDL3)

add_library(forge_world STATIC
    src/engine/world/scenery/store.cpp
    src/engine/world/sectors/sectors.cpp
    src/engine/world/spatial/frustum.cpp
    src/engine/world/spatial/cull.cpp
    src/engine/world/scenery/city.cpp
)
target_include_directories(forge_world PUBLIC src)
target_link_libraries(forge_world PUBLIC glm::glm)

# Dependency DAG (do not invert):
#   forge_net  (no game types)
#   forge_game -> forge_net
#   forge_script -> forge_game
#   forge_game_net -> forge_net + forge_game + forge_script + forge_physics
#   forge_anim -> forge_assets
#   forge_engine -> camera/assets/anim/world/rhi  (not game_net)
# Labs/game binaries link forge_engine + forge_game_net as needed.
add_library(forge_engine STATIC
    src/engine/app/run.cpp
    src/engine/render/passes/post/composite.cpp
    src/engine/rhi/device/resources.cpp
    src/engine/rhi/buffer/buffer.cpp
    src/engine/rhi/device/upload.cpp
    src/engine/rhi/swapchain/swapchain.cpp
    src/engine/rhi/sampler/sampler.cpp
    src/engine/render/passes/ui/backdrop.cpp
    src/engine/frontend/settings/settings.cpp
    src/engine/frontend/menu/menu.cpp
    src/engine/frontend/inventory/inventory_menu.cpp
    src/engine/frontend/menu/background.cpp
    src/engine/frontend/menu/menu_scene.cpp
    src/engine/ui/text/font.cpp
    src/engine/ui/widgets/ui.cpp
    src/engine/core/paths/paths.cpp
    src/engine/core/jobs/jobs.cpp
    src/engine/dev/overlay.cpp
    src/engine/platform/window/host.cpp
    src/engine/rhi/shader/shader.cpp
    src/engine/rhi/texture/texture.cpp
    src/engine/render/renderer/ibl.cpp
    src/engine/render/passes/opaque/pbr_pass.cpp
    src/engine/render/passes/opaque/pbr_scene.cpp
    src/engine/render/renderer/survival_visuals.cpp
    src/engine/render/framegraph/framegraph.cpp
    src/engine/render/renderer/renderer.cpp
    src/engine/audio/device/device.cpp
)
target_include_directories(forge_engine PUBLIC src)
target_link_libraries(forge_engine PUBLIC SDL3::SDL3 forge_camera forge_assets forge_anim forge_asset_deps forge_world ktx_read forge_imgui)
target_compile_definitions(forge_engine PUBLIC
    FORGE_VERSION="${PROJECT_VERSION}"
    FORGE_DEV_UI=$<BOOL:${FORGE_DEV_UI}>
    FORGE_GPU_DEBUG=$<IF:$<CONFIG:Debug>,1,0>
)

add_library(forge_physics STATIC src/engine/physics/world/world.cpp src/engine/physics/locomotion/als.cpp)
target_include_directories(forge_physics PUBLIC src)
target_link_libraries(forge_physics PUBLIC Jolt glm::glm)

add_library(forge_net STATIC
    src/engine/net/transport/socket.cpp
    src/engine/net/transport/stream.cpp
    src/engine/net/connection/connection.cpp
    src/engine/net/transport/channel.cpp
)
target_include_directories(forge_net PUBLIC src)
if(WIN32)
    target_link_libraries(forge_net PUBLIC ws2_32)
endif()
if(MSVC)
    target_compile_options(forge_net PRIVATE /W4 /permissive-)
else()
    target_compile_options(forge_net PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(forge_game STATIC
    src/engine/game/session/session.cpp
    src/engine/game/actors/actors.cpp
    src/engine/game/systems/save.cpp
)
target_include_directories(forge_game PUBLIC src)
target_link_libraries(forge_game PUBLIC glm::glm forge_net)
if(MSVC)
    target_compile_options(forge_game PRIVATE /W4 /permissive-)
else()
    target_compile_options(forge_game PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(forge_script STATIC
    src/engine/script/bindings/api.cpp
    src/engine/script/vm/vm.cpp
)
target_include_directories(forge_script PUBLIC src)
target_link_libraries(forge_script PUBLIC lua_static glm::glm forge_camera forge_game)
if(MSVC)
    target_compile_options(forge_script PRIVATE /W4 /permissive-)
else()
    target_compile_options(forge_script PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(forge_game_net STATIC
    src/engine/game_net/snapshots/packets.cpp
    src/engine/game_net/interest/interest.cpp
    src/engine/game_net/server/server.cpp
    src/engine/game_net/client/client.cpp
    src/engine/game_net/prediction/prediction.cpp
    src/engine/game_net/snapshots/interpolation.cpp
)
target_include_directories(forge_game_net PUBLIC src)
target_link_libraries(forge_game_net PUBLIC forge_net forge_script forge_game forge_physics glm::glm)
if(MSVC)
    target_compile_options(forge_game_net PRIVATE /W4 /permissive-)
else()
    target_compile_options(forge_game_net PRIVATE -Wall -Wextra -Wpedantic)
endif()

file(GLOB FORGE_HELMET_RUNTIME
    ${CMAKE_SOURCE_DIR}/assets/models/FlightHelmet/*.gltf
    ${CMAKE_SOURCE_DIR}/assets/models/FlightHelmet/*.bin
    ${CMAKE_SOURCE_DIR}/assets/models/FlightHelmet/*.ktx2
    ${CMAKE_SOURCE_DIR}/assets/models/FlightHelmet/LICENSE.md
)

add_custom_target(forge_shaders ALL
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/cook_shaders.py --check
    COMMENT "Verify cooked shaders"
)

add_custom_target(forge_stage_runtime
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets/models/Als
        ${CMAKE_BINARY_DIR}/assets/models/Als
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets/models/Nature
        ${CMAKE_BINARY_DIR}/assets/models/Nature
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets/models/Campfire
        ${CMAKE_BINARY_DIR}/assets/models/Campfire
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets/effects
        ${CMAKE_BINARY_DIR}/assets/effects
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders/cooked
        ${CMAKE_BINARY_DIR}/shaders
    COMMAND ${CMAKE_COMMAND} -E make_directory
        ${CMAKE_BINARY_DIR}/assets/models/FlightHelmet
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${FORGE_HELMET_RUNTIME}
        ${CMAKE_BINARY_DIR}/assets/models/FlightHelmet
    COMMAND ${CMAKE_COMMAND} -E make_directory
        ${CMAKE_BINARY_DIR}/assets/models/Fox
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/assets/models/Fox/Fox.glb
        ${CMAKE_SOURCE_DIR}/assets/models/Fox/LICENSE.md
        ${CMAKE_SOURCE_DIR}/assets/models/Fox/SOURCE.txt
        ${CMAKE_BINARY_DIR}/assets/models/Fox
    COMMAND ${CMAKE_COMMAND} -E make_directory
        ${CMAKE_BINARY_DIR}/assets/fonts
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/assets/fonts/DroidSans.ttf
        ${CMAKE_SOURCE_DIR}/assets/fonts/LICENSE.txt
        ${CMAKE_BINARY_DIR}/assets/fonts
    COMMAND ${CMAKE_COMMAND} -E make_directory
        ${CMAKE_BINARY_DIR}/assets/menu
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/assets/menu/background.png
        ${CMAKE_SOURCE_DIR}/assets/menu/SOURCE.txt
        ${CMAKE_BINARY_DIR}/assets/menu
    COMMAND ${CMAKE_COMMAND} -E make_directory
        ${CMAKE_BINARY_DIR}/scripts
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/assets/scripts/main.lua
        ${CMAKE_SOURCE_DIR}/assets/scripts/net.lua
        ${CMAKE_BINARY_DIR}/scripts
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets/models/SurvivalKit
        ${CMAKE_BINARY_DIR}/assets/models/SurvivalKit
    COMMENT "Stage shaders and assets"
)
add_dependencies(forge_stage_runtime forge_shaders)

function(forge_lab target)
    add_executable(${target} ${ARGN})
    target_link_libraries(${target} PRIVATE forge_engine)
    target_include_directories(${target} PRIVATE src)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_dependencies(${target} forge_stage_runtime)
    if(WIN32)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:${target}> $<TARGET_FILE_DIR:${target}>
            COMMAND_EXPAND_LISTS
        )
    endif()
endfunction()

if(MSVC)
    target_compile_options(forge_engine PRIVATE /W4 /permissive-)
    target_compile_options(forge_camera PRIVATE /W4 /permissive-)
    target_compile_options(forge_assets PRIVATE /W4 /permissive-)
    target_compile_options(forge_anim PRIVATE /W4 /permissive-)
    target_compile_options(forge_world PRIVATE /W4 /permissive-)
else()
    target_compile_options(forge_engine PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(forge_camera PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(forge_assets PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(forge_anim PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(forge_world PRIVATE -Wall -Wextra -Wpedantic)
endif()

forge_lab(forge_m1 tests/m1/main.cpp tests/m1/lab.cpp)
forge_lab(forge_m2 tests/m2/main.cpp tests/m2/lab.cpp)
forge_lab(forge_m3 tests/m3/main.cpp tests/m3/lab.cpp)
forge_lab(forge_m4 tests/m4/main.cpp tests/m4/lab.cpp)
forge_lab(forge_m5 tests/m5/main.cpp tests/m5/lab.cpp)
forge_lab(forge_m6 tests/m6/main.cpp tests/m6/lab.cpp)
forge_lab(forge_m7 tests/m7/main.cpp tests/m7/lab.cpp)
forge_lab(forge_survival tests/survival/main.cpp tests/survival/lab.cpp tests/survival/shell.cpp)
forge_lab(forge tests/survival/main.cpp tests/survival/lab.cpp tests/survival/shell.cpp)
target_link_libraries(forge_m3 PRIVATE forge_physics)
target_link_libraries(forge_m5 PRIVATE forge_physics)
target_link_libraries(forge_m6 PRIVATE forge_physics forge_script)
target_link_libraries(forge_m7 PRIVATE forge_game_net)
target_link_libraries(forge_survival PRIVATE forge_game_net)
target_link_libraries(forge PRIVATE forge_game_net)

if(BUILD_TESTING)
    add_executable(camera_tests tests/camera_tests.cpp)
    target_link_libraries(camera_tests PRIVATE forge_camera)
    add_test(NAME camera COMMAND camera_tests)

    add_executable(gltf_tests tests/gltf_tests.cpp)
    target_link_libraries(gltf_tests PRIVATE forge_assets)
    add_test(NAME gltf COMMAND gltf_tests)
    set_tests_properties(gltf PROPERTIES ENVIRONMENT "FORGE_TEST_ROOT=${CMAKE_SOURCE_DIR}")

    add_executable(world_tests tests/world_tests.cpp)
    target_link_libraries(world_tests PRIVATE forge_world forge_camera)
    add_test(NAME world COMMAND world_tests)

    add_executable(skin_tests tests/skin_tests.cpp)
    target_link_libraries(skin_tests PRIVATE forge_anim)
    add_test(NAME skin COMMAND skin_tests)
    set_tests_properties(skin PROPERTIES ENVIRONMENT "FORGE_TEST_ROOT=${CMAKE_SOURCE_DIR}")

    add_executable(script_tests tests/script_tests.cpp)
    target_link_libraries(script_tests PRIVATE forge_script)
    add_test(NAME script COMMAND script_tests)

    add_executable(net_tests tests/net_tests.cpp)
    target_link_libraries(net_tests PRIVATE forge_game_net)
    add_test(NAME net COMMAND net_tests)

    add_executable(economy_tests tests/economy_tests.cpp)
    target_link_libraries(economy_tests PRIVATE forge_game)
    add_test(NAME economy COMMAND economy_tests)

    add_executable(survival_tests tests/survival_tests.cpp)
    target_link_libraries(survival_tests PRIVATE forge_game_net)
    add_test(NAME survival COMMAND survival_tests)

    add_executable(locomotion_tests tests/locomotion_tests.cpp)
    target_link_libraries(locomotion_tests PRIVATE forge_anim forge_physics)
    add_test(NAME locomotion COMMAND locomotion_tests)
    set_tests_properties(locomotion PROPERTIES ENVIRONMENT "FORGE_TEST_ROOT=${CMAKE_SOURCE_DIR}")

    add_executable(phys_tests tests/phys_tests.cpp)
    target_link_libraries(phys_tests PRIVATE forge_physics)
    add_test(NAME phys COMMAND phys_tests)

    add_executable(settings_tests tests/settings_tests.cpp)
    target_link_libraries(settings_tests PRIVATE forge_engine)
    add_test(NAME settings COMMAND settings_tests)

    add_executable(ui_tests tests/ui_tests.cpp)
    target_link_libraries(ui_tests PRIVATE forge_engine)
    add_test(NAME ui_font COMMAND ui_tests)

    add_executable(app_tests tests/app_tests.cpp)
    target_link_libraries(app_tests PRIVATE forge_engine forge_game_net)
    add_test(NAME app COMMAND app_tests)

    add_executable(arch_tests tests/arch_tests.cpp)
    target_link_libraries(arch_tests PRIVATE forge_engine forge_game_net forge_world)
    add_test(NAME arch COMMAND arch_tests)
    set_tests_properties(ui_font PROPERTIES WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

    add_test(NAME shader_assets COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/cook_shaders.py --check)
    add_test(NAME shader_asset_failures COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/shader_assets_test.py)
    add_test(NAME texture_assets COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/cook_textures.py --check)
    if(FORGE_GPU_TESTS)
        add_test(NAME gpu_smoke_m1 COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/smoke.py $<TARGET_FILE:forge_m1>)
        add_test(NAME gpu_smoke_m2 COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/smoke.py $<TARGET_FILE:forge_m2>)
        add_test(NAME gpu_smoke_m3 COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/smoke.py $<TARGET_FILE:forge_m3>)
        add_test(NAME gpu_smoke_m4 COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/smoke.py $<TARGET_FILE:forge_m4>)
        add_test(NAME gpu_smoke_m5 COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/smoke.py $<TARGET_FILE:forge_m5>)
        add_test(NAME gpu_smoke_m6 COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/smoke.py $<TARGET_FILE:forge_m6>)
        add_test(NAME gpu_smoke_m7 COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/smoke.py $<TARGET_FILE:forge_m7>)
        add_test(NAME gpu_smoke_survival COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/smoke.py $<TARGET_FILE:forge>)
        set_tests_properties(gpu_smoke_m1 gpu_smoke_m2 gpu_smoke_m3 gpu_smoke_m4 gpu_smoke_m5 gpu_smoke_m6 gpu_smoke_m7 gpu_smoke_survival PROPERTIES TIMEOUT 120 LABELS gpu RUN_SERIAL TRUE)
    endif()
endif()
