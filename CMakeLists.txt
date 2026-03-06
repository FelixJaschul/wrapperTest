cmake_minimum_required(VERSION 3.20)
project(implc)
set(CMAKE_CXX_STANDARD 20)

set(EXAMPLE_DIRS_SDL
        "examples/sdl_gpu_voxel"
        "examples/sdl_gpu_triangle"
        "examples/sdl_cpu_triangle"
        "examples/sdl_cpu_model"
        "examples/sdl_gpu_model"
)

set(EXAMPLE_DIRS_X11
        "examples/x11_cpu_triangle"
        "examples/x11_cpu_model"
)

# Architecture (Apple)
if(APPLE)
    if(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "arm64" AND NOT DEFINED CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build architecture" FORCE)
    endif()
endif()

# X11
find_package(X11 REQUIRED)
message(STATUS "X11 found at ${X11_INCLUDE_DIR}")

# SDL3
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS        OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES     OFF CACHE BOOL "" FORCE)
add_subdirectory(wrapper/libs/SDL EXCLUDE_FROM_ALL)

# ImGui
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/wrapper/libs/imgui)
add_library(imgui STATIC
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
        ${IMGUI_DIR}/backends/imgui_impl_sdlgpu3.cpp
        ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer3.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(imgui PUBLIC SDL3::SDL3)

# Shader compilation
add_custom_target(generate_shaders
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/build_shaders.sh ${CMAKE_BINARY_DIR}/shaders
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating GPU shaders (SPIR-V + optional MSL/DXIL via SDL_shadercross)"
        VERBATIM
)

# Build each SDL example
foreach(example_dir ${EXAMPLE_DIRS_SDL})
    get_filename_component(example_name ${example_dir} NAME)
    set(source_file "${CMAKE_CURRENT_SOURCE_DIR}/${example_dir}/${example_name}.c")

    if(NOT EXISTS "${source_file}")
        message(WARNING "Skipping '${example_name}': source not found at ${source_file}")
        continue()
    endif()

    message(STATUS "Configuring SDL example: ${example_name}")

    add_executable(${example_name} ${source_file})
    set_source_files_properties(${source_file} PROPERTIES LANGUAGE CXX)

    target_include_directories(${example_name} PRIVATE
            wrapper
            wrapper/libs/imgui
            wrapper/libs/imgui/backends
    )

    target_link_libraries(${example_name} PRIVATE
            imgui
            SDL3::SDL3
    )

    add_dependencies(${example_name} generate_shaders)
endforeach()

# Build each X11 example
foreach(example_dir ${EXAMPLE_DIRS_X11})
    get_filename_component(example_name ${example_dir} NAME)
    set(source_file "${CMAKE_CURRENT_SOURCE_DIR}/${example_dir}/${example_name}.c")

    if(NOT EXISTS "${source_file}")
        message(WARNING "Skipping '${example_name}': source not found at ${source_file}")
        continue()
    endif()

    message(STATUS "Configuring X11 example: ${example_name}")

    add_executable(${example_name} ${source_file})
    set_source_files_properties(${source_file} PROPERTIES LANGUAGE CXX)

    target_include_directories(${example_name} PRIVATE
            wrapper
            ${X11_INCLUDE_DIR}
    )

    target_link_libraries(${example_name} PRIVATE ${X11_LIBRARIES})

    add_dependencies(${example_name} generate_shaders)
endforeach()
