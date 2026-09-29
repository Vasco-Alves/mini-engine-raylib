# 2. Define Source Files
set(SOURCES
    "src/assets/assets.cpp"
    "src/audio/audio.cpp"
    "src/core/engine.cpp"
    "src/core/time.cpp"
    "src/input/input.cpp"
    "src/input/input_defaults.cpp"
    "src/render/renderer.cpp"
    "src/systems/camera_system.cpp"
    "src/scripting/script_manager.cpp"
    "src/systems/script_system.cpp"
    "src/scene/scene_manager.cpp"
    "src/systems/transform_system.cpp"
    "src/core/vfs.cpp" 
    "src/core/logger.cpp"
    "src/systems/physics_system.cpp"
    "src/ecs/component_registry.cpp"
  "src/systems/audio_system.cpp" "src/systems/raytracer_system.cpp" "src/render/raytracing_math.cpp")

# 3. Create the Library
add_library(engine STATIC ${SOURCES})

# sol2's template instantiations overflow MSVC's default COFF section limit.
if(MSVC)
    set_source_files_properties("src/scripting/script_manager.cpp" PROPERTIES COMPILE_OPTIONS "/bigobj")
endif()

# The CPU path tracer uses std::execution::par_unseq. On Linux, libstdc++ runs it
# on oneTBB whenever the TBB headers are installed, and the final link then needs
# TBB as well. Link it when present; otherwise pin libstdc++ to its serial backend
# so the build never depends on TBB (the CPU tracer then runs single-threaded).
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(TBB QUIET)
    if(TBB_FOUND)
        target_link_libraries(engine PRIVATE TBB::tbb)
    else()
        target_compile_definitions(engine PRIVATE _GLIBCXX_USE_TBB_PAR_BACKEND=0)
        message(WARNING "TBB not found: the CPU path tracer will run single-threaded. "
                        "Install TBB (e.g. libtbb-dev) for multithreaded CPU rendering.")
    endif()
endif()

# Version header, generated from the root project(VERSION ...) so there is exactly
# one place to bump it. Written into the build tree, never the source tree.
configure_file(
    "include/mini-engine-raylib/core/version.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/mini-engine-raylib/core/version.hpp"
    @ONLY
)

# 4. Setup Include Directories
target_include_directories(engine PUBLIC 
    "include" 
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
    "${CMAKE_SOURCE_DIR}/vendor/sol3/include"
    "${CMAKE_SOURCE_DIR}/vendor/lua/src"
    "${CMAKE_SOURCE_DIR}/vendor/JoltPhysics"
    
    # --- SHORTCUT DIRECTORIES ---
    "include/mini-engine-raylib"
    "include/mini-engine-raylib/assets"
    "include/mini-engine-raylib/audio"
    "include/mini-engine-raylib/core"
    "include/mini-engine-raylib/ecs"
    "include/mini-engine-raylib/input"
    "include/mini-engine-raylib/render"
    "include/mini-engine-raylib/scene"
    "include/mini-engine-raylib/systems"
)

# 5. Link Dependencies
target_link_libraries(engine 
    PRIVATE raylib 
    PUBLIC nlohmann_json::nlohmann_json mini-ecs lua Jolt
)