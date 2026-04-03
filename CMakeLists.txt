cmake_minimum_required(VERSION 4.2)
project(rheo-sph)

include(FetchContent)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(glfw3 REQUIRED)
find_package(Vulkan REQUIRED)

FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 0af55ccecd98d4e5a8d1fad7de25ba429d60e863 #refs/tags/1.0.1
)

FetchContent_MakeAvailable(glm)

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
        main.cpp
        window.h
        window.cpp
        renderer.h
        renderer.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE glfw Vulkan::cppm glm::glm)

find_program(SLANGC_EXECUTABLE slangc HINTS $ENV{VULKAN_SDK}/bin REQUIRED)

function(compile_slang_shaders TARGET_NAME SHADER_FILE OUTPUT_DIR OUTPUT_SPIRV ENTRY_POINTS_LIST)
        get_filename_component(OUTPUT_SPIRV_NAME "${OUTPUT_SPIRV}" NAME)
        add_custom_command(
                OUTPUT ${OUTPUT_DIR}
                COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}
        )
        add_custom_command(
                OUTPUT ${OUTPUT_SPIRV}
                COMMAND ${SLANGC_EXECUTABLE} ${SHADER_FILE} -target spirv -profile spirv_1_4+spvRayQueryKHR -emit-spirv-directly -fvk-use-entrypoint-name ${ENTRY_POINTS_LIST} -o ${OUTPUT_SPIRV_NAME}
                WORKING_DIRECTORY ${OUTPUT_DIR}
                DEPENDS ${OUTPUT_DIR} ${SHADER_FILE}
                COMMENT "Compiling Slang Shaders: ${SHADER_FILE}"
                VERBATIM
        )
        add_custom_target(${TARGET_NAME} DEPENDS ${OUTPUT_SPIRV})
endfunction()

file(GLOB SHADER_SOURCES bucket.slang)
set(SHADERS_DIR shaders)
set(ENTRY_POINTS -entry fluid_bucket -entry wall_bucket -entry clear_bucket)

compile_slang_shaders(slang_shader "${SHADER_SOURCES}" "${SHADERS_DIR}" "${SHADERS_DIR}/bucket.spv" "${ENTRY_POINTS}")
add_dependencies(${PROJECT_NAME} slang_shader)
