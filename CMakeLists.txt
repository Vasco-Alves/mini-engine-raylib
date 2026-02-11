cmake_minimum_required(VERSION 3.20)

project(
    "mini-engine-raylib"
    VERSION 0.4.1
    DESCRIPTION "A simple game engine built with Raylib"
    LANGUAGES CXX
)

# C++ Standard configuration
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(engine)

# Running "MiniEngine" directly (Dev Mode)
if (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR) 
    option(BUILD_SANDBOX "Build the sandbox application" ON)
    if (BUILD_SANDBOX)
        add_subdirectory(sandbox)
    endif()

# Being included by another game (User Mode)
else()
    message(STATUS "MiniEngine included as a library. Sandbox disabled.")

endif()
