cmake_minimum_required(VERSION 3.20)

project(
    "mini-engine"
    VERSION 0.3.0
    DESCRIPTION "A simple 2D game engine built with Raylib"
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

if (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    
    # We are running "MiniEngine" directly (Dev Mode)
    option(BUILD_SANDBOX "Build the sandbox application" ON)

    if (BUILD_SANDBOX)
        add_subdirectory(sandbox)
    endif()

else()
    
    # We are being included by another game (User Mode)
    message(STATUS "MiniEngine included as a library. Sandbox disabled.")

endif()
