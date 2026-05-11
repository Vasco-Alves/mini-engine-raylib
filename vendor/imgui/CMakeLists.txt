cmake_minimum_required(VERSION 3.20)
project(imgui CXX)

# Grab all the C++ files in this folder
file(GLOB SOURCES "*.cpp" "*.h")

# Create the library
add_library(imgui STATIC ${SOURCES})

# Link it to vcpkg's raylib
target_link_libraries(imgui PUBLIC raylib)

# Let the editor see the headers
target_include_directories(imgui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")