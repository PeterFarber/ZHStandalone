# Optional vcpkg toolchain (manifest: game/vcpkg.json).
# Set VCPKG_ROOT or pass -DCMAKE_TOOLCHAIN_FILE=.../vcpkg.cmake from scripts/build*.bat.

function(zh_init_vcpkg_toolchain)
    if(DEFINED CMAKE_TOOLCHAIN_FILE)
        message(STATUS "Using CMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}")
        return()
    endif()

    if(DEFINED ENV{VCPKG_ROOT})
        set(_vcpkg_root "$ENV{VCPKG_ROOT}")
    elseif(EXISTS "$ENV{USERPROFILE}/vcpkg/scripts/buildsystems/vcpkg.cmake")
        set(_vcpkg_root "$ENV{USERPROFILE}/vcpkg")
    endif()

    if(DEFINED _vcpkg_root)
        set(_toolchain "${_vcpkg_root}/scripts/buildsystems/vcpkg.cmake")
        if(EXISTS "${_toolchain}")
            set(CMAKE_TOOLCHAIN_FILE "${_toolchain}" CACHE STRING "vcpkg toolchain" FORCE)
            message(STATUS "vcpkg toolchain: ${_toolchain}")
        endif()
    endif()
endfunction()

function(zh_find_glslc out_var)
    set(_hints "")
    if(DEFINED ENV{VULKAN_SDK})
        list(APPEND _hints "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin")
    endif()
    if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
        list(APPEND _hints
            "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/tools/shaderc"
            "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/tools/glslang")
    endif()
    find_program(_glslc glslc HINTS ${_hints})
    set(${out_var} "${_glslc}" PARENT_SCOPE)
endfunction()
