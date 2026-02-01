# 1. Find Dependencies
find_package(raylib CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# 2. Define Source Files 
set(SOURCES
    "src/Assets.cpp"
    "src/Audio.cpp"
    "src/CameraSystem.cpp"
    "src/Engine.cpp"
    "src/Entity.cpp"
    "src/GameApp.cpp"
    "src/Input.cpp"
    "src/InputDefaults.cpp"
    "src/LifetimeSystem.cpp"
    "src/Registry.cpp"
    "src/Render3D.cpp"
    "src/Scene.cpp"
    "src/SceneManager.cpp"
    "src/Time.cpp"
)

# 3. Define Header Files
set(HEADERS
    "include/Assets.hpp"
    "include/Audio.hpp"
    "include/CameraSystem.hpp"
    "include/Color.hpp"
    "include/Components.hpp"
    "include/Engine.hpp"
    "include/Entity.hpp"
    "include/GameApp.hpp"
    "include/GameScene.hpp"
    "include/Input.hpp"
    "include/InputDefaults.hpp"
    "include/LifetimeSystem.hpp"
    "include/Math.hpp"
    "include/Pool.hpp"
    "include/Registry.hpp"
    "include/Render3D.hpp"
    "include/Scene.hpp"
    "include/SceneManager.hpp"
    "include/Time.hpp"
)

# 4. Create the Library
add_library(engine STATIC ${SOURCES} ${HEADERS})

# 5. Setup Include Directories
target_include_directories(engine PUBLIC "include")

# 6. Link Dependencies
target_link_libraries(engine 
    PRIVATE raylib 
    PUBLIC nlohmann_json::nlohmann_json
)