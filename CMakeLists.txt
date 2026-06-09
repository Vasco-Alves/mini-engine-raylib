cmake_minimum_required(VERSION 3.5)

set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)

project(
    "mini-engine-raylib"
    VERSION 0.11.2
    DESCRIPTION "A simple game engine built with Raylib"
    LANGUAGES CXX
)

# Force Static Runtime Globally
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# OVERRIDE LEGACY CMAKE FLAGS
if(MSVC)
    set(variables
        CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
    )
    foreach(variable ${variables})
        string(REGEX REPLACE "/MD" "/MT" ${variable} "${${variable}}")
    endforeach()
endif()

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
# VENDOR LIBRARIES (Git Submodules)
# ==========================================

# Raylib
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
add_definitions(-DGRAPHICS_API_OPENGL_43)
add_subdirectory(vendor/raylib)

# nlohmann_json
set(JSON_BuildTests OFF CACHE INTERNAL "")
add_subdirectory(vendor/nlohmann_json)

# Local Vendors
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
set(USE_STATIC_MSVC_RUNTIME_LIBRARY ON CACHE BOOL "Force Jolt to use Static" FORCE)

# OVERRIDE_CXX_FLAGS=ON (Jolt's default) replaces CMAKE_CXX_FLAGS_DEBUG with its
# own flags that strip the debug-CRT markers, causing _ITERATOR_DEBUG_LEVEL=0 in
# Jolt while the rest of the project uses IDL=2 in Debug — a hard linker error.
# Turning it OFF lets Jolt inherit the project's standard debug/release flags.
set(OVERRIDE_CXX_FLAGS OFF CACHE BOOL "Prevent Jolt from replacing project CXX flags" FORCE)

# Jolt's /Wall /WX spills into the build diagnostics; silence it.
set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "Disable Jolt strict warnings" FORCE)

add_subdirectory(vendor/joltphysics/Build)

# ==========================================
# INTERNAL TARGETS
# ==========================================
add_subdirectory(engine)
add_subdirectory(editor)