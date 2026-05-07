cmake_minimum_required(VERSION 3.20)
project(imgui CXX)

# Grab all ImGui and rlImGui source files
file(GLOB SOURCES "*.cpp" "*.h")

add_library(imgui STATIC ${SOURCES})

# rlImGui needs to know about Raylib, so we link it here
target_link_libraries(imgui PRIVATE raylib)

# Expose the folder so our Editor can #include <imgui.h>
target_include_directories(imgui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")