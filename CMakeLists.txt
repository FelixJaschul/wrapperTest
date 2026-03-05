cmake_minimum_required(VERSION 3.20)
project(implc)
set(CMAKE_CXX_STANDARD 20)

if(APPLE)
    if(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "arm64" AND NOT DEFINED CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build architecture" FORCE)
    endif()
endif()

set(EXAMPLE_DIRS "examples/sdl_gpu_voxel"
                 "examples/sdl_gpu_triangle" "examples/sdl_cpu_triangle")

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

# Common include dirs and link libraries for all examples
set(COMMON_INCLUDE_DIRS wrapper wrapper/libs/imgui wrapper/libs/imgui/backends)
set(COMMON_LINK_LIBS imgui SDL3::SDL3)

# Build each example
foreach(example_dir ${EXAMPLE_DIRS})
    # Extract the example name from the directory path
    get_filename_component(example_name ${example_dir} NAME)
    
    # Source file path
    set(source_file "${CMAKE_CURRENT_SOURCE_DIR}/${example_dir}/${example_name}.c")
    
    # Skip if source file doesn't exist
    if(NOT EXISTS "${source_file}")
        message(WARNING "Source file ${source_file} not found, skipping example '${example_name}'")
        continue()
    endif()
    
    # Create executable
    add_executable(${example_name} ${source_file})
    set_source_files_properties(${source_file} PROPERTIES LANGUAGE CXX)
    target_include_directories(${example_name} PRIVATE ${COMMON_INCLUDE_DIRS})
    target_link_libraries(${example_name} PRIVATE ${COMMON_LINK_LIBS})
    
    # Platform-specific optimizations
    if(APPLE AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
        target_compile_options(${example_name} PRIVATE
            -O3
            -march=armv8.5-a
            -mtune=apple-m2
            -ffast-math
        )
    endif()
endforeach()

# Shader compilation - runs before all examples
add_custom_target(generate_shaders
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/build_shaders.sh ${CMAKE_BINARY_DIR}/shaders
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating GPU shaders (SPIR-V + optional MSL/DXIL via SDL_shadercross)"
    VERBATIM
)

# Make all examples depend on shader generation
foreach(example_dir ${EXAMPLE_DIRS})
    get_filename_component(example_name ${example_dir} NAME)
    if(TARGET ${example_name})
        add_dependencies(${example_name} generate_shaders)
    endif()
endforeach()
