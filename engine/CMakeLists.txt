# 1. Find Dependencies
find_package(raylib CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# 2. Define Source Files (Lowercase and GameApp.cpp removed)
set(SOURCES
    "src/assets/assets.cpp"
    "src/audio/audio.cpp"
    "src/core/engine.cpp"
    "src/core/time.cpp"
    "src/input/input.cpp"
    "src/input/input_defaults.cpp"
    "src/render/renderer.cpp"
    "src/render/camera_system.cpp"
    "src/scene/scene.cpp"
)

# 3. Create the Library
add_library(engine STATIC ${SOURCES})

# 4. Setup Include Directories
target_include_directories(engine PUBLIC 
    "include" # Exposes the namespace folder so users can do: #include <mini-engine-raylib/core/engine.hpp>
    
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
    PUBLIC nlohmann_json::nlohmann_json mini-ecs
)