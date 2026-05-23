cmake_minimum_required(VERSION 3.20)

project(
    "mini-engine-raylib"
    VERSION 0.10.0
    DESCRIPTION "A simple game engine built with Raylib"
    LANGUAGES CXX
)

# --- Force everything to use MT/MTd (Static) (Jolt Physics) ---
set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# C++ Standard configuration
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ==========================================
# VENDOR LIBRARIES
# ==========================================
add_subdirectory(vendor/mini-ecs)
add_subdirectory(vendor/lua)
add_subdirectory(vendor/imgui)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/vendor/lua/src")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/vendor/sol3/include")

# Jolt Physics 
set(TARGET_UNIT_TESTS OFF CACHE BOOL "Don't build Jolt unit tests" FORCE)
set(TARGET_HELLO_WORLD OFF CACHE BOOL "Don't build Jolt hello world" FORCE)
set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "Don't build Jolt performance test" FORCE)
set(USE_AVX2 ON CACHE BOOL "Use AVX2 instructions for physics math" FORCE)

add_subdirectory(vendor/JoltPhysics/Build)

# ==========================================
# INTERNAL TARGETS
# ==========================================

# Build engine
add_subdirectory(engine)
add_subdirectory(editor)

# If running "MiniEngine" directly (Dev Mode) build sandbox
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR) 
    option(BUILD_SANDBOX "Build the sandbox application" ON)
    if (BUILD_SANDBOX)
        add_subdirectory(sandbox)
    endif()
# Being included by another game (User Mode)
else()
    message(STATUS "MiniEngine included as a library. Sandbox disabled.")
endif()
