# Mini-Engine-Raylib

Originally built as a lightweight wrapper over [Raylib](https://www.raylib.com/), the engine has evolved into a robust development environment. It features a custom Data-Oriented Entity-Component-System (ECS), a AAA physics backend, live Lua scripting, spatial audio, and a complete UI editor, making it an ideal platform for graphics research and rapid prototyping.

> Version 0.11.2 · C++23 · Windows (MSVC) and Linux (GCC)

![Editor](docs/images/editor_hero.png)

## Features

- **ECS core** — entities are plain data; systems iterate component pools([mini-ecs](vendor/mini-ecs)).
- **Editor** — dockable ImGui UI with scene hierarchy, inspector, content browser, console, and a viewport with ImGuizmo move/rotate/scale gizmos and mouse picking.
- **Scene serialization** — scenes are JSON; projects have their own asset folders.
- **Lua scripting** — per-entity scripts with `start`/`update` callbacks and hot-reload ([sol3](vendor/sol3) + Lua).
- **Physics** — rigid bodies and box/sphere colliders via [Jolt](vendor/joltphysics).
- **Rendering** — forward renderer with a custom lighting shader (point + directional lights, basic PBR-ish material params), plus a CPU/GPU path tracer for offline-quality stills.
- **Audio** — sound effects and streaming music with simple 3D spatialization.
- **Undo/redo** — command-history stack wired through the inspector and gizmos.

## Getting Started

### Requirements

- **C++23** compatible compiler (MSVC, GCC or Clang)
- **CMake 3.5+**

### Cloning the Repository

This project uses Git submodules for core engine components. You **must** clone it recursively to fetch all the required source code.

```bash
git clone --recursive https://github.com/YOUR_USERNAME/mini-engine-raylib.git
cd mini-engine-raylib
```

*(If you already cloned it normally, run `git submodule update --init --recursive` to pull the missing files).*

### With CMake presets (recommended)

```bash
cmake --preset x64-debug        # or: x64-release, linux-debug
cmake --build --preset x64-debug
```

Presets use the Ninja generator. The `editor` executable lands in
`out/build/<preset>/bin/`, with `assets/` copied next to it automatically.

### Manual configure

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Architecture

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the layering, the per-frame system
order, the serialization model, and a checklist for adding a new component type.

## Submodules

- [**Mini-ECS**](https://github.com/Vasco-Alves/mini-ecs): A custom-built, lightweight Entity-Component-System engineered specifically for this engine to handle contiguous memory pools and fast component iteration.

- [**Raylib**](https://github.com/raysan5/raylib): Core windowing, input processing, and rendering context.
- [**Dear ImGui**](https://github.com/ocornut/imgui) & **ImGuizmo**: Powers the entire Editor UI (Inspector, Hierarchy, Viewports) via `rlImGui`.
- [**Jolt Physics**](https://github.com/jrouwe/JoltPhysics): Multi-threaded AAA 3D collision and rigidbody simulation.
- [**sol3**](https://github.com/ThePhD/sol2): A C++ bindings library that bridges the engine architecture to the Lua scripting environment.
- [**Portable File Dialogs**](https://github.com/samhocevar/portable-file-dialogs): Opens native GUI file dialogs depending on the OS.

## Future Improvements & Thesis Roadmap

While the engine is highly capable, there is always room to grow and optimize. Future updates will pivot heavily toward advanced rendering techniques:

- **Integrated Raytracer / Path-Tracer:** Implementing an offline or compute-shader-based rendering mode to generate physically accurate, raytraced images directly from the scene.
