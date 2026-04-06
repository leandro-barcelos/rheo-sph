cmake_minimum_required(VERSION 4.2)
project(rheo-sph)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(glfw3 REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(Vulkan REQUIRED)

add_library(VulkanCppModule INTERFACE)
add_library(Vulkan::cppm ALIAS VulkanCppModule)
target_link_libraries(VulkanCppModule INTERFACE Vulkan::Vulkan)
target_compile_definitions(VulkanCppModule INTERFACE
        VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
        VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1
)

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME}
        PRIVATE
        src/main.cpp
        src/window.h
        src/window.cpp
        src/renderer.h
        src/renderer.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE glfw Vulkan::cppm glm::glm)

if(UNIX AND NOT APPLE)
  target_link_libraries(${PROJECT_NAME} PRIVATE dl)
endif()

find_program(SLANGC_EXECUTABLE slangc HINTS $ENV{VULKAN_SDK}/bin REQUIRED)

function(compile_slang_shaders TARGET_NAME SHADER_FILE OUTPUT_DIR OUTPUT_SPIRV)
  # Remaining args are forwarded directly to slangc (e.g. -entry foo -entry bar)
  set(ENTRY_ARGS ${ARGN})

  get_filename_component(SHADER_FILE_ABS "${SHADER_FILE}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")

  add_custom_command(
                OUTPUT ${OUTPUT_SPIRV}
                COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}
                COMMAND ${SLANGC_EXECUTABLE} ${SHADER_FILE_ABS}
                -target spirv
                -profile spirv_1_4+spvRayQueryKHR
                -emit-spirv-directly
                -fvk-use-entrypoint-name
                ${ENTRY_ARGS}
                -o ${OUTPUT_SPIRV}
                DEPENDS ${SHADER_FILE_ABS}
                COMMENT "Compiling Slang Shaders: ${SHADER_FILE}"
                VERBATIM
        )
  add_custom_target(${TARGET_NAME} DEPENDS ${OUTPUT_SPIRV})
endfunction()

set(SHADERS_DIR shaders)

compile_slang_shaders(bucket_compute_shader "shaders/compute/bucket.slang" "${SHADERS_DIR}" "${SHADERS_DIR}/compute/bucket.spv"
        -entry fluid_bucket
        -entry wall_bucket
        -entry clear_bucket
)
compile_slang_shaders(particle_render_shader "shaders/graphics/particle.slang" "${SHADERS_DIR}" "${SHADERS_DIR}/graphics/particle.spv"
        -entry vert_main
        -entry frag_main
)
add_dependencies(${PROJECT_NAME} bucket_compute_shader particle_render_shader)
