# MiniEngineRaylib

**MiniEngineRaylib** is a lightweight game engine written in **C++**, built on top of [Raylib](https://www.raylib.com/).  
It is designed to be simple, minimal, and educational — ideal for learning engine architecture or building small games.

## Features

- **Entity–Component System (ECS)** Lightweight architecture with easy-to-use entity API and registry management.

- **Scene Serialization** Save and load scenes to JSON (`scene.json`) with entities and components, powered by [nlohmann/json](https://github.com/nlohmann/json).

- **Input System** Bind digital axes and actions (e.g., "MoveX", "MoveY", "Quit", etc.).

- **Time Management** Frame delta and fixed-step support for consistent physics updates.

- **Camera System** Follow targets smoothly and manage multiple viewports.

- **Asset Management** Texture loading with reference counting and automatic cleanup.

## Getting Started

### Requirements

- **C++20** compatible compiler
- **CMake 3.20+**
- **[vcpkg](https://github.com/microsoft/vcpkg)** (for dependency management)
- **Ninja** (optional, recommended for fast builds)

### Building

This project uses **CMake** and **vcpkg** to handle dependencies (Raylib & nlohmann-json) automatically.

#### 1. Setup vcpkg

Ensure you have the `VCPKG_ROOT` environment variable set, or know where your vcpkg installation is.

#### 2. Configure & Build

**Using Command Line:**

```bash
# configure (assumes 'vcpkg' is in your path or VCPKG_ROOT is set)
cmake -B out/build -S . -DCMAKE_TOOLCHAIN_FILE="$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"

# build
cmake --build out/build
```

**Using Visual Studio:**

1. Open the folder mini-engine.
2. CMake Presets should automatically detect the toolchain (see CMakePresets.json).
3. Select a preset (e.g., x64-debug) and hit Build.

## Running the Example

The executable is built into ```out/build/<preset>/bin/```. Asset files are automatically copied to the output directory.

```bash
./out/build/x64-debug/bin/sandbox.exe
```

## Future Improvements & Roadmap

MiniEngineRaylib is intentionally small and educational, but there’s plenty of room to grow. Below are posible ideas for future iterations and contributions:

### Core Systems

- Component reflection system (automatic save/load of all registered components)
- Prefab & entity archetype support
- Hierarchical transforms (parent/child relationships)
- Improved input mapping (mouse, gamepad, rebindable actions)
- Event system (publish/subscribe pattern)

### Rendering

- Texture atlases & batching
- Parallax layers / tilemaps
- Lighting and simple 2D shaders

### Engine Tools

- In-engine inspector (entity/component viewer)
- Scene editor UI (ImGui-based)
- Live reloading of assets and scripts

### Audio & FX

- Positional 2D audio sources
- Background music management
- Basic mixer and volume control per category (music, SFX, UI)

## Integrations

While MiniEngineRaylib currently focuses on minimal custom systems, it leverages robust industry standards where it matters:

- **[Raylib](https://github.com/raysan5/raylib)**: Windowing, Audio, and Rendering backend.

- **[nlohmann/json](https://github.com/nlohmann/json)**: Scene serialization and data parsing.

Possible future dependencies:

- **[EnTT](https://github.com/skypjack/entt)** – A high-performance, modern C++ Entity-Component-System library.  
  Could replace or back MiniEngineRaylib’s custom ECS for large-scale projects.

- **[Box2D](https://github.com/erincatto/box2d)** – A proven 2D rigid-body physics engine.  
  Ideal for replacing the simple AABB system with proper physics simulation (forces, impulses, collisions, joints).

- **[sol2](https://github.com/ThePhD/sol2)** or **[LuaBridge](https://github.com/vinniefalco/LuaBridge)** – For adding Lua scripting support, enabling rapid gameplay logic prototyping.

- **[Dear ImGui](https://github.com/ocornut/imgui)** – For building an in-engine editor and debugging tools (entity inspector, scene hierarchy, profiler, etc.).

- **[spdlog](https://github.com/gabime/spdlog)** or **[fmt](https://github.com/fmtlib/fmt)** – For better logging and string formatting utilities.

- **[nlohmann/json](https://github.com/nlohmann/json)** *(already used)* – May remain the backbone for scene serialization and data exchange.
