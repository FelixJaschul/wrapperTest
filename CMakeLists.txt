cmake_minimum_required(VERSION 3.20)
project(test)
set(CMAKE_CXX_STANDARD 20)

if(APPLE)
    if(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "arm64" AND NOT DEFINED CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build architecture" FORCE)
    endif()
endif()

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
    ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer3.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(imgui PUBLIC SDL3::SDL3)

add_executable(test main.c)
set_source_files_properties(main.c PROPERTIES LANGUAGE CXX)  # compile .c as C++
target_include_directories(test PRIVATE wrapper wrapper/libs/imgui wrapper/libs/imgui/backends)
target_link_libraries(test PRIVATE imgui SDL3::SDL3)

add_custom_target(generate_shaders
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/shaders/build_shaders.sh
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating GPU shaders (SPIR-V + optional MSL/DXIL via SDL_shadercross)"
    VERBATIM
)
add_dependencies(test generate_shaders)

if(APPLE AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    target_compile_options(test PRIVATE
        -O3
        -march=armv8.5-a
        -mtune=apple-m2
        -ffast-math
    )
endif()
