cmake_minimum_required(VERSION 3.20)

project(zh_game LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(ZhVcpkg)
include(ZhExternals)
include(ZhShaders)

foreach(_cfg Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${_cfg}" _ucfg)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_ucfg} "${CMAKE_CURRENT_SOURCE_DIR}/bin/${_cfg}")
endforeach()

file(GLOB_RECURSE ZH_GAME_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
list(FILTER ZH_GAME_SOURCES EXCLUDE REGEX "/platform/")
list(FILTER ZH_GAME_SOURCES EXCLUDE REGEX "/gfx/")

list(APPEND ZH_GAME_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/window_glfw.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/input_glfw.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/paths_win32.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gfx/gfx_api.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gfx/gfx_compat.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gfx/gfx_projection.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gfx/vulkan/vk_util.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gfx/vulkan/vk_matrix.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gfx/vulkan/vk_draw2d.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gfx/vulkan/vk_draw3d.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gfx/vulkan/font_bytes.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gfx/vulkan/vk_font.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gfx/vulkan/vk_context.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gfx/vulkan/vk_swapchain.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gfx/vulkan/vk_renderer.cpp")

add_executable(zh_game ${ZH_GAME_SOURCES})
set_target_properties(zh_game PROPERTIES OUTPUT_NAME "zh_game")

target_include_directories(zh_game PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src")

zh_link_game_externals(zh_game)
zh_add_vulkan_gfx(zh_game)
zh_compile_vk_shaders(ZH_SPIRV_OUTPUTS)
if(TARGET zh_shaders)
    add_dependencies(zh_game zh_shaders)
endif()

if(MSVC)
    target_compile_options(zh_game PRIVATE /W4 /Zc:__cplusplus /permissive-)
    target_compile_definitions(zh_game PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _WINSOCK_DEPRECATED_NO_WARNINGS
        NOMINMAX
        WIN32_LEAN_AND_MEAN)
    target_link_options(zh_game PRIVATE
        "$<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>,$<CONFIG:MinSizeRel>>:/SUBSYSTEM:WINDOWS>"
        "$<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>,$<CONFIG:MinSizeRel>>:/ENTRY:mainCRTStartup>")
else()
    target_compile_options(zh_game PRIVATE -Wall -Wextra)
endif()

if(WIN32)
    target_compile_definitions(zh_game PRIVATE _WIN32)
endif()

add_custom_command(TARGET zh_game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/resources"
        "$<TARGET_FILE_DIR:zh_game>/resources"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/maps"
        "$<TARGET_FILE_DIR:zh_game>/maps"
    COMMENT "Copy resources/ and maps/ to output directory")
