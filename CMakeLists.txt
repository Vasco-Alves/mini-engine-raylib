cmake_minimum_required(VERSION 3.21)

set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)

project(
    "mini-engine-raylib"
    VERSION 1.0.0
    DESCRIPTION "A small scene engine built on raylib: play scenes as games or path-trace them into film"
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
# The GPU path tracer needs compute shaders + SSBOs, which require OpenGL 4.3. Use raylib's
# own OPENGL_VERSION knob so it compiles the 4.3 code paths AND requests a 4.3 context at
# window creation, and propagates GRAPHICS_API_OPENGL_43 (PUBLIC) to the engine/editor/imgui.
# macOS caps OpenGL at 4.1 — no compute shaders there, so the GPU path-tracer backend is unavailable on Apple platforms (the raytracer detects the missing compute program at runtime and the CPU backend keeps working).
if(APPLE)
    set(OPENGL_VERSION "3.3" CACHE STRING "OpenGL version (macOS has no 4.3)" FORCE)
else()
    set(OPENGL_VERSION "4.3" CACHE STRING "OpenGL version (4.3 required for compute shaders)" FORCE)
endif()
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
# AVX2 only exists on x86-64 — Apple Silicon / ARM builds must not request it.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    set(USE_AVX2 ON CACHE BOOL "Use AVX2 instructions for physics math" FORCE)
else()
    set(USE_AVX2 OFF CACHE BOOL "AVX2 unavailable on this architecture" FORCE)
endif()
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
add_subdirectory(game)

# ==========================================
# TESTS
# ==========================================
enable_testing()
add_subdirectory(tests)

# ==========================================
# PACKAGING — `cpack -G ZIP -C Release` in the build dir produces a
# redistributable zip: editor + game runtime + assets + docs. The "runtime"
# component keeps vendored libraries' install rules out of the archive.
# ==========================================
install(TARGETS editor game RUNTIME DESTINATION . COMPONENT runtime)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/engine/assets/" DESTINATION assets COMPONENT runtime)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/editor/assets/" DESTINATION assets COMPONENT runtime)
install(FILES README.md LICENSE CHANGELOG.md DESTINATION . COMPONENT runtime)

set(CPACK_GENERATOR ZIP)
set(CPACK_PACKAGE_NAME "mini-engine-raylib")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_FILE_NAME "mini-engine-raylib-${PROJECT_VERSION}-win64")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
set(CPACK_COMPONENTS_ALL runtime)
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
# Single-component archive: name it like the package, not "<package>-runtime".
set(CPACK_ARCHIVE_RUNTIME_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}")
include(CPack)