cmake_minimum_required(VERSION 4.2)
project(rheo-sph)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(glfw3 REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(GDAL REQUIRED)

if(NOT GDAL_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GDAL REQUIRED gdal)
endif()

add_library(VulkanCppModule INTERFACE)
add_library(Vulkan::cppm ALIAS VulkanCppModule)
target_link_libraries(VulkanCppModule INTERFACE Vulkan::Vulkan)
target_compile_definitions(
    VulkanCppModule
    INTERFACE
        VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
        VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1
)

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui)
set(IMGUI_FILE_DIALOG_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui-file-dialog
)

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
    ${IMGUI_FILE_DIALOG_DIR}/ImGuiFileDialog.cpp
)

add_library(imgui STATIC ${IMGUI_SOURCES})

target_include_directories(
    imgui
    PUBLIC
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
        ${Vulkan_INCLUDE_DIRS}
        ${IMGUI_FILE_DIALOG_DIR}
)

target_link_libraries(imgui PUBLIC Vulkan::Vulkan glfw)

target_compile_definitions(imgui PUBLIC VK_USE_PLATFORM_XLIB_KHR)

set(IMGUI_DIR external/imgui)
include_directories(${IMGUI_DIR} ${IMGUI_DIR}/backends ..)

add_executable(${PROJECT_NAME})

target_sources(
    ${PROJECT_NAME}
    PRIVATE
        src/main.cpp
        src/app/simulation_session.cpp
        src/app/simulation_session.h
        src/app/ui_controller.cpp
        src/app/ui_controller.h
        src/app/rheo_sph_app.cpp
        src/app/rheo_sph_app.h
        src/core/command_pool.cpp
        src/core/command_pool.h
        src/core/frame_sync.cpp
        src/core/frame_sync.h
        src/core/input_events.h
        src/core/pipeline.cpp
        src/core/pipeline.h
        src/core/vulkan_context.cpp
        src/core/vulkan_context.h
        src/core/vulkan_device.cpp
        src/core/vulkan_device.h
        src/core/vulkan_swap_chain.cpp
        src/core/vulkan_swap_chain.h
        src/core/window.cpp
        src/core/window.h
        src/renderer/camera.cpp
        src/renderer/camera.h
        src/renderer/fluid_renderer.cpp
        src/renderer/fluid_renderer.h
        src/renderer/renderer.cpp
        src/renderer/renderer.h
        src/renderer/ui_texture_handle.h
        src/renderer/ui_texture_registry.cpp
        src/renderer/ui_texture_registry.h
        src/resources/buffer.cpp
        src/resources/buffer.h
        src/resources/descriptor.cpp
        src/resources/descriptor.h
        src/resources/geotiff.cpp
        src/resources/geotiff.h
        src/resources/immediate_submit.cpp
        src/resources/immediate_submit.h
        src/resources/images.cpp
        src/resources/images.h
        src/resources/memory.cpp
        src/resources/memory.h
        src/simulation/fluid_simulator.cpp
        src/simulation/fluid_simulator.h
        src/ui/imgui_layer.cpp
        src/ui/imgui_layer.h
        src/ui/panels/menu_bar_panel.cpp
        src/ui/panels/menu_bar_panel.h
        src/ui/panels/parameters_panel.cpp
        src/ui/panels/parameters_panel.h
        src/ui/panels/top_bar_panel.cpp
        src/ui/panels/top_bar_panel.h
)

target_include_directories(${PROJECT_NAME} PRIVATE external)
target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE glfw Vulkan::cppm glm::glm imgui yaml-cpp GDAL::GDAL
)

if(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE dl)
endif()

find_program(SLANGC_EXECUTABLE slangc HINTS $ENV{VULKAN_SDK}/bin REQUIRED)

function(compile_slang_shaders TARGET_NAME SHADER_FILE OUTPUT_DIR OUTPUT_SPIRV)
    # Remaining args are forwarded directly to slangc (e.g. -entry foo -entry bar)
    set(ENTRY_ARGS ${ARGN})

    get_filename_component(
        SHADER_FILE_ABS
        "${SHADER_FILE}"
        ABSOLUTE
        BASE_DIR "${CMAKE_SOURCE_DIR}"
    )

    add_custom_command(
        OUTPUT ${OUTPUT_SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}
        COMMAND
            ${SLANGC_EXECUTABLE} ${SHADER_FILE_ABS} -target spirv -profile
            spirv_1_4+spvRayQueryKHR -emit-spirv-directly
            -fvk-use-entrypoint-name ${ENTRY_ARGS} -o ${OUTPUT_SPIRV}
        DEPENDS ${SHADER_FILE_ABS}
        COMMENT "Compiling Slang Shaders: ${SHADER_FILE}"
        VERBATIM
    )
    add_custom_target(${TARGET_NAME} DEPENDS ${OUTPUT_SPIRV})
endfunction()

set(SHADERS_DIR shaders)

compile_slang_shaders(bucket_compute_shader "shaders/compute/bucket.slang" "${SHADERS_DIR}/compute" "${SHADERS_DIR}/compute/bucket.spv"
        -entry fluid_bucket
        -entry wall_bucket
        -entry clear_bucket
)
compile_slang_shaders(density_compute_shader "shaders/compute/density.slang" "${SHADERS_DIR}/compute" "${SHADERS_DIR}/compute/density.spv"
        -entry comp_main
)
compile_slang_shaders(vel_pos_compute_shader "shaders/compute/vel_pos.slang" "${SHADERS_DIR}/compute" "${SHADERS_DIR}/compute/vel_pos.spv"
        -entry comp_main
)
compile_slang_shaders(particle_render_shader "shaders/graphics/particle.slang" "${SHADERS_DIR}/graphics" "${SHADERS_DIR}/graphics/particle.spv"
        -entry vert_main
        -entry frag_main
)
add_dependencies(
    ${PROJECT_NAME}
    bucket_compute_shader
    density_compute_shader
    vel_pos_compute_shader
    particle_render_shader
)
