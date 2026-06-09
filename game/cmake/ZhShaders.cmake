function(zh_compile_vk_shaders out_var)
    set(shader_dir "${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders/vk")
    set(spv_outputs "")

    zh_find_glslc(ZH_GLSLC)

    if(NOT ZH_GLSLC)
        message(STATUS "glslc not found — install vcpkg package 'shaderc' or LunarG Vulkan SDK")
        message(STATUS "SPIR-V shaders must be prebuilt in resources/shaders/vk/*.spv")
        set(${out_var} "" PARENT_SCOPE)
        return()
    endif()

    message(STATUS "glslc: ${ZH_GLSLC}")

    file(GLOB ZH_SHADER_SOURCES CONFIGURE_DEPENDS
        "${shader_dir}/*.vert"
        "${shader_dir}/*.frag")

    foreach(src IN LISTS ZH_SHADER_SOURCES)
        get_filename_component(name "${src}" NAME_WE)
        get_filename_component(ext "${src}" EXT)
        set(spv "${shader_dir}/${name}${ext}.spv")
        add_custom_command(
            OUTPUT "${spv}"
            COMMAND "${ZH_GLSLC}" "${src}" -o "${spv}"
            DEPENDS "${src}"
            COMMENT "SPIR-V ${name}${ext}"
            VERBATIM)
        list(APPEND spv_outputs "${spv}")
    endforeach()

    if(spv_outputs)
        add_custom_target(zh_shaders DEPENDS ${spv_outputs})
    endif()
    set(${out_var} "${spv_outputs}" PARENT_SCOPE)
endfunction()
