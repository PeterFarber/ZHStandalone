# Vendored deps under build/external (populated by scripts/fetch_externals.ps1 or .sh).

set(ZH_EXTERNAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build/external" CACHE PATH "Third-party source root")

function(zh_require_external path desc)
    if(NOT EXISTS "${ZH_EXTERNAL_DIR}/${path}")
        message(FATAL_ERROR
            "Missing ${desc}: ${ZH_EXTERNAL_DIR}/${path}\n"
            "Run: game/scripts/fetch_externals.sh (Linux/macOS) or fetch_externals.ps1 (Windows)")
    endif()
endfunction()

zh_require_external("enet/include/enet/enet.h" "enet")
zh_require_external("nlohmann/include/nlohmann/json.hpp" "nlohmann/json")
zh_require_external("miniaudio/miniaudio.h" "miniaudio")
zh_require_external("stb/stb_truetype.h" "stb")

# --- enet -------------------------------------------------------------------
file(GLOB ZH_ENET_SOURCES CONFIGURE_DEPENDS "${ZH_EXTERNAL_DIR}/enet/*.c")
add_library(zh_enet STATIC ${ZH_ENET_SOURCES})
target_include_directories(zh_enet PUBLIC "${ZH_EXTERNAL_DIR}/enet/include")
target_compile_definitions(zh_enet PUBLIC ENET_ENABLED=1)
if(WIN32)
    target_compile_definitions(zh_enet PRIVATE WIN32)
    target_link_libraries(zh_enet PUBLIC ws2_32 winmm)
    set_source_files_properties("${ZH_EXTERNAL_DIR}/enet/unix.c" PROPERTIES HEADER_FILE_ONLY TRUE)
else()
    set_source_files_properties("${ZH_EXTERNAL_DIR}/enet/win32.c" PROPERTIES HEADER_FILE_ONLY TRUE)
    target_link_libraries(zh_enet PUBLIC pthread)
endif()

function(zh_link_game_externals target)
    target_include_directories(${target} PRIVATE
        "${ZH_EXTERNAL_DIR}/nlohmann/include"
        "${ZH_EXTERNAL_DIR}/miniaudio"
        "${ZH_EXTERNAL_DIR}/stb")
    target_link_libraries(${target} PRIVATE zh_enet)
    if(WIN32)
        target_link_libraries(${target} PRIVATE ws2_32 winmm shell32 user32)
    endif()
endfunction()

# --- GLFW + Vulkan (vcpkg manifest: vulkan-headers, vulkan-loader, glfw3, shaderc) ----
function(zh_add_vulkan_gfx target)
    find_package(Vulkan REQUIRED)

    find_package(glfw3 CONFIG QUIET)
    if(glfw3_FOUND)
        set(_glfw_target glfw)
        if(TARGET glfw3::glfw)
            set(_glfw_target glfw3::glfw)
        endif()
        message(STATUS "GLFW: vcpkg/system (glfw3)")
    else()
        include(FetchContent)
        set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(
            glfw
            GIT_REPOSITORY https://github.com/glfw/glfw.git
            GIT_TAG 3.4
            GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(glfw)
        set(_glfw_target glfw)
        message(STATUS "GLFW: FetchContent fallback (run setup_vcpkg.ps1 for vcpkg glfw3)")
    endif()

    if(EXISTS "${ZH_EXTERNAL_DIR}/volk/volk.h")
        target_include_directories(${target} PRIVATE "${ZH_EXTERNAL_DIR}/volk")
        target_compile_definitions(${target} PRIVATE VOLK_IMPLEMENTATION)
    endif()
    if(EXISTS "${ZH_EXTERNAL_DIR}/glm/glm/glm.hpp")
        target_include_directories(${target} PRIVATE "${ZH_EXTERNAL_DIR}/glm")
    elseif(EXISTS "${ZH_EXTERNAL_DIR}/glm/glm-1.0.1/glm/glm.hpp")
        target_include_directories(${target} PRIVATE "${ZH_EXTERNAL_DIR}/glm/glm-1.0.1")
    endif()

    target_link_libraries(${target} PRIVATE Vulkan::Vulkan ${_glfw_target})
    target_compile_definitions(${target} PRIVATE ZH_GFX_VULKAN=1 GLFW_INCLUDE_NONE)

    zh_link_vcpkg_woff2(${target})
endfunction()

function(zh_link_vcpkg_woff2 target)
    if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
        set(_woff2_prefix "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/include/woff2/decode.h")
        set(_woff2_prefix "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/build/cmake-vk/vcpkg_installed/x64-linux/include/woff2/decode.h")
        set(_woff2_prefix "${CMAKE_CURRENT_SOURCE_DIR}/build/cmake-vk/vcpkg_installed/x64-linux")
    else()
        message(STATUS "woff2: not found (WOFF2 fonts require vcpkg manifest install)")
        return()
    endif()

    if(NOT EXISTS "${_woff2_prefix}/include/woff2/decode.h")
        message(STATUS "woff2: headers missing at ${_woff2_prefix}")
        return()
    endif()

    target_include_directories(${target} PRIVATE "${_woff2_prefix}/include")
    target_compile_definitions(${target} PRIVATE ZH_HAVE_WOFF2=1)

    if(WIN32)
        target_link_libraries(${target} PRIVATE
            "$<$<CONFIG:Debug>:${_woff2_prefix}/debug/lib/woff2dec.lib>"
            "$<$<NOT:$<CONFIG:Debug>>:${_woff2_prefix}/lib/woff2dec.lib>"
            "$<$<CONFIG:Debug>:${_woff2_prefix}/debug/lib/woff2common.lib>"
            "$<$<NOT:$<CONFIG:Debug>>:${_woff2_prefix}/lib/woff2common.lib>"
            "$<$<CONFIG:Debug>:${_woff2_prefix}/debug/lib/brotlidec.lib>"
            "$<$<NOT:$<CONFIG:Debug>>:${_woff2_prefix}/lib/brotlidec.lib>"
            "$<$<CONFIG:Debug>:${_woff2_prefix}/debug/lib/brotlicommon.lib>"
            "$<$<NOT:$<CONFIG:Debug>>:${_woff2_prefix}/lib/brotlicommon.lib>")
    else()
        find_library(_zh_woff2dec woff2dec HINTS "${_woff2_prefix}/lib")
        find_library(_zh_woff2common woff2common HINTS "${_woff2_prefix}/lib")
        find_library(_zh_brotlidec brotlidec HINTS "${_woff2_prefix}/lib")
        find_library(_zh_brotlicommon brotlicommon HINTS "${_woff2_prefix}/lib")
        if(NOT _zh_woff2dec OR NOT _zh_woff2common)
            message(STATUS "woff2: libraries missing at ${_woff2_prefix}/lib")
            return()
        endif()
        target_link_libraries(${target} PRIVATE
            ${_zh_woff2dec} ${_zh_woff2common} ${_zh_brotlidec} ${_zh_brotlicommon})
    endif()
    message(STATUS "woff2: ${_woff2_prefix}")
endfunction()
