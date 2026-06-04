# MiniEngineRaylib

**MiniEngineRaylib** is a cross-platform modular, 3D game engine and editor written in **C++23**.

Originally built as a lightweight wrapper over [Raylib](https://www.raylib.com/), the engine has evolved into a robust development environment. It features a custom Data-Oriented Entity-Component-System (ECS), a AAA physics backend, live Lua scripting, spatial audio, and a complete UI editor, making it an ideal platform for graphics research and rapid prototyping.

## Core Features

- **Robust Editor Architecture:** A fully dockable ImGui interface featuring a Scene Hierarchy, Entity Inspector, and interactive 3D ImGuizmo manipulation. Features standard QoL tools including **Drag-and-Drop** asset loading and a complete **Undo/Redo** Command history system.
- **Advanced Camera Controls:** Smooth viewport navigation with Free-Fly (FPS style) and Object Orbit (Alt + Click) camera modes, backed by precise 3D mouse-picking/raycasting.
- **Entity-Component-System (ECS):** A lightweight, contiguous-memory architecture that seamlessly syncs visual data, physics bounds, audio, and logic in real-time.
- **GPU-Accelerated 3D Rendering:** A custom Blinn-Phong shader pipeline supporting multi-light architecture (Directional and Point lights) with physically based attenuation.
- **Jolt Physics Backend:** Professional-grade 3D physics integration supporting Dynamic, Kinematic, and Static rigidbodies, alongside Box and Sphere colliders with real-time debug wireframes.
- **3D Spatial Audio:** Integrated audio pipeline supporting background music streams and spatialized audio sources with distance falloff, pitch manipulation, and active listeners.
- **Lua Scripting & Hot-Reloading:** Write gameplay logic in Lua using `Sol3`. The engine detects file changes and hot-reloads scripts instantly without recompiling the C++ core.
- **Data-Oriented Scene Serialization:** Save and load complete scenes (including all components and script states) to JSON.

## Getting Started

### Requirements

- **C++23** compatible compiler (MSVC, GCC or Clang)
- **CMake 3.5+**
- **Git**
- **Visual Studio** (Highly Recommended)

### 1. Cloning the Repository

This project uses Git submodules for core engine components. You **must** clone it recursively to fetch all the required source code.

```bash
git clone --recursive https://github.com/Vasco-Alves/mini-engine-raylib.git
cd mini-engine-raylib
```

*(If you already cloned it normally, run `git submodule update --init --recursive` to pull the missing files).*

### 2. Building the Engine (Visual Studio)

The engine handles its dependencies internally via modern CMake, so no external package managers are required.

1. Open **Visual Studio**.
2. Select **"Open a local folder"** and select the cloned `mini-engine-raylib` directory.
3. Allow CMake a few moments to automatically generate the cache.
4. In the top toolbar, select `editor.exe` as your Startup Item.
5. Select x64 Release. 
6. Hit **Build and Run**. Asset files and default shaders are automatically copied to the output directory during the build process.

## Architecture & Dependencies

MiniEngineRaylib bridges a custom data-oriented core with robust industry standards to create a seamless development pipeline:

### Submodules

- [**Mini-ECS**](https://github.com/Vasco-Alves/mini-ecs): A custom-built, lightweight Entity-Component-System engineered specifically for this engine to handle contiguous memory pools and fast component iteration.

### External Libraries

- [**Raylib**](https://github.com/raysan5/raylib): Core windowing, input processing, and rendering context.
- [**Dear ImGui**](https://github.com/ocornut/imgui) & **ImGuizmo**: Powers the entire Editor UI (Inspector, Hierarchy, Viewports) via `rlImGui`.
- [**Jolt Physics**](https://github.com/jrouwe/JoltPhysics): Multi-threaded AAA 3D collision and rigidbody simulation.
- [**sol3**](https://github.com/ThePhD/sol2): A C++ bindings library that bridges the engine architecture to the Lua scripting environment.
- [**Portable File Dialogs**](https://github.com/samhocevar/portable-file-dialogs): Opens native GUI file dialogs depending on the OS.
- 

## Future Improvements & Thesis Roadmap

While the engine is highly capable, there is always room to grow and optimize. Future updates will pivot heavily toward advanced rendering techniques:

- **Integrated Raytracer / Path-Tracer:** Implementing an offline or compute-shader-based rendering mode to generate physically accurate, raytraced images directly from the scene.
