# Changelog

All notable changes to this project will be documented in this file.

# Untitled

## [0.10.0] - 2026-05-24

A monumental update that transforms the engine from a static visualizer into a fully interactive simulation. This release integrates a AAA physics backend (Jolt Physics), establishes a bidirectional bridge between the ECS and the physics world, and introduces a hot-reloading Lua scripting environment to drive gameplay logic.

### Physics Engine & ECS Integration

- **Jolt Physics Integration:** Successfully integrated the Jolt Physics backend. The engine now manages dedicated physics lifecycles (`init`, `on_play`, `on_stop`, `shutdown`) and dynamically calculates thread pooling (`hardware_concurrency`) to prevent zero-thread crashes on low-core machines.
- **Rigid Body Dynamics:** Introduced the `RigidBodyComponent` with full support for `Dynamic`, `Static`, and `Kinematic` motion types. The physics system automatically calculates inertia, overrides zero-mass dynamics to prevent divide-by-zero crashes, and seamlessly synchronizes mathematical reality back to the visual `TransformComponent` every frame.
- **Collision Primitives:** Added `BoxColliderComponent` and `SphereColliderComponent`. Colliders automatically inherit scale from the entity's transform and feature built-in safety nets that block the creation of illegal zero-thickness physics shapes.

### Editor UX & Physics Tooling

- **Real-Time Physics Debug Renderer:** Added a custom OpenGL matrix-hacking debug drawer that overlays exact physics hitboxes onto visual meshes. Hitboxes are color-coded by state (Green for Dynamic, Red for Static, Blue for Kinematic) to allow level designers to read the simulation at a glance.
- **Per-Collider Debug Toggles:** The Inspector now features a `show_debug` toggle for all collision components, allowing users to isolate and debug specific hitboxes without cluttering the entire viewport.
- **Deterministic Debug Stepping:** Upgraded the Editor's Play/Pause state machine. Pressing the "Step" button while the simulation is paused now feeds the physics engine a hardcoded fixed timestep (`1/60th` of a second), allowing for perfectly predictable, frame-by-frame physics debugging.
- **Non-Fatal Asserts:** Overwrote Jolt's internal `AssertFailedImpl`. Mathematical errors (like invalid quaternions) now gracefully report to the Editor Console as red `[JOLT ASSERT]` logs instead of triggering a C++ `__debugbreak()` and crashing the application.

### Lua Scripting & Input Bridge

- **Lua Hot-Reloading Environment:** Integrated `Sol3` to build a sandboxed Lua scripting pipeline. The `ScriptComponent` now tracks file modification timestamps, allowing gameplay programmers to save a `.lua` file in VS Code and instantly see the changes execute in the engine without recompiling.
- **The Physics-Script Bridge:** Exposed physical impulse commands to the Lua environment. Scripts can now call `entity:set_velocity(x, y, z)` to safely wake up Jolt bodies and apply forces directly to the ECS.
- **Action/Axis Input Manager:** Completely decoupled hardcoded key presses into a professional Action/Axis mapping system. Lua scripts now query logical events (e.g., `Input.action_pressed("Jump")`), paving the way for future custom keybindings.

### Scene Management & Logging

- **State Transition Logging:** The Editor Toolbar now broadcasts state changes directly to the console (`Mode set to: PLAYING` / `EDIT`), drastically improving feedback when hot-swapping between simulation and level design modes.

## [0.9.0] - 2026-05-23

A massive overhaul of the visual pipeline, replacing Raylib's default rendering with a custom GPU-accelerated Blinn-Phong lighting system. This update significantly upgrades the Editor's visual fidelity and introduces professional-grade graphics debugging tools.

### Graphics & Lighting Pipeline

- **Custom Blinn-Phong Shader:** Replaced the default rasterizer with a custom GLSL vertex and fragment shader pipeline. The engine now calculates accurate diffuse and specular highlights based on the camera's view direction and the normal vectors of 3D models.
- **Multi-Light Architecture:** The renderer now extracts lighting data from the ECS and injects it into GPU uniform arrays, supporting up to 8 simultaneous `LightComponent` point lights per scene with physically based distance attenuation (falloff).
- **Directional Global Illumination:** Added a `DirectionalLightComponent` (The Sun). The shader calculates the forward vector from the Entity's rotation matrix and applies a baseline Ambient Light pass to simulate basic Global Illumination, preventing pitch-black shadows on unlit faces.

### Editor UX & Graphics Tooling

- **Lit / Unlit Render Modes:** Added a professional viewport toggle (`View -> Lit Mode`). The engine now caches Raylib's default internal shader on boot, allowing level designers to hot-swap materials and bypass lighting math to debug raw geometry.
- **Visual Editor Gizmos:** Invisible entities now have a physical presence in the editor. Point lights render as wireframe spheres, and Directional Lights render as spheres with dynamic vector arrows that perfectly track the light's orientation in real-time.
- **Gizmo Raycasting:** Upgraded the Viewport's 3D mouse-picking algorithm. The raycaster now successfully detects collisions against Editor Gizmos, allowing users to click and select Lights directly in the 3D scene view instead of relying solely on the Hierarchy panel.
- **Default Scene Overhaul:** Pressing `Ctrl+N` now spawns a mathematically correct mid-day lighting setup (a Directional Light pitched at -45 degrees) rather than a floating point light, ensuring new scenes are instantly ready for DCC asset placement.

### Scene Management & Serialization

- **Data-Oriented Serialization:** Upgraded the `scene_manager` to support the expanded ECS. The JSON serializer now correctly reads/writes state data for the new `LightComponent`, `DirectionalLightComponent`, `CameraComponent`, `Camera2DComponent`, and `Shape2DComponent`.
- **Entity Duplication Polish:** Bulletproofed the `Ctrl+D` duplication shortcut. The copy logic now deeply clones all new rendering and lighting components, and correctly ignores ImGui hover states to prevent the shortcut from silently failing when the mouse is over the viewport.

## [0.8.0] - 2026-05-22

A massive architectural refactor introducing decoupled communication, AAA-standard Editor QoL features, and real-time Hot-Reloading, solidifying the engine as a professional Digital Content Creation (DCC) tool.

### Engine Architecture & Performance

- **Type-Erased Event Bus:** Implemented a robust Publish/Subscribe `EventBus` using variadic templates and `std::type_index`. Subsystems (like Audio, Physics, and Editor UI) are now strictly decoupled, communicating via `LogEvent`, `SceneLoadedEvent`, and `PlayStateChangedEvent` without circular dependencies.
- **Time Controls (Pause & Step):** Upgraded the engine's core loop with time manipulation. Added Pause and Step-Forward functionality, allowing the engine to freeze the `dt` passed to Lua scripts and Physics while keeping the Editor Camera fully active.
- **TRS Matrix Optimization:** Fixed a severe performance bottleneck where the renderer was calculating Transform matrices every frame. Matrix multiplication (Scale -> Rotation -> Translation) is now isolated within the `TransformSystem` and safely gated behind an `is_dirty` flag and `Vector3Equals` checks.

### Editor UX & QoL Polish

- **DPI-Aware UI Scaling:** Replaced hardcoded pixel sizes with dynamic `ImGui::GetFrameHeight()` math. The Editor UI now perfectly scales from 75% to 200% without clipping, and the user's preferred scale is persistently saved in `engine_config.json`.
- **Advanced Drag & Drop:** - **OS-Level Import:** The engine intercepts Windows Explorer `IsFileDropped()` events, seamlessly cloning externally dragged files directly into the active project's `assets/` directory.
    - **Internal Payload Routing:** Implemented `ImGuiPayload` to allow dragging 3D models and `.lua` scripts from the Content Browser directly onto Entity components in the Inspector.
- **Console Panel:** Built a standalone terminal window that subscribes to internal engine logs. Features auto-scrolling, warning/error color coding, and visual separation for `[LUA]` specific print outputs.
- **Hierarchy & Inspector Polish:** - **Context Menus:** Added Right-Click menus to the Hierarchy for creating empty entities and deleting existing ones.
    - **Collapsible Components:** Upgraded the Inspector with `ImGuiTreeNodeFlags_Framed`, turning components into clean accordion menus while maintaining right-aligned delete buttons.
- **Viewport Tools:** Added an infinite 3D grid (`DrawGrid`) to ground the scene visually. Implemented `Ctrl + D` for instant Entity duplication, and the `'F'` key shortcut to instantly calculate pitch/yaw math and snap the Editor Camera to the selected object.

### Scripting & Tooling

- **Lua Hot-Reloading:** The engine now polls the OS for `.lua` file timestamps twice a second. Saving a script in an external IDE (like VS Code) triggers an instant, seamless memory wipe and reload of the Lua environment *while the game is running*.
- **Syntax Error Trapping:** A broken Lua script will no longer crash the C++ core. Syntax errors are caught via `sol::error` and neatly piped to the Editor's Console Panel as red text.
- **Multi-Script Support:** Modified the `ScriptComponent` to hold a `std::vector` of script instances, allowing a single entity to run multiple decoupled Lua behaviors simultaneously.

### Bug Fixes & Memory Safety

- **Asset Memory Leaks:** Fixed a GPU leak in the Inspector. Deleting a `Model3DComponent` or overriding it via drag-and-drop now correctly fires `me::assets::release()` to decrement the reference counter and safely unload the asset from VRAM.
- **ECS Iterator Invalidation:** Prevented mid-frame memory crashes by implementing a deferred deletion tracker (`s_EntityToDelete`) for the UI, and reverse-loop iteration in the `ScriptSystem` to safely handle entities calling `destroy()` on themselves.

## [0.7.1] - 2026-05-12
Major upgrades to the Editor UX, Asset Management, and Project Workflow, transitioning the engine into a true multi-project software tool.

### Project Management & VFS
- **Project Hub:** Decoupled game data from the CMake build folder. The engine now boots to a Hub that allows creating or loading Unity-style project directories (assets/scenes, assets/models, etc.) safely on the user's hard drive.
- **Native OS Dialogs:** Integrated portable-file-dialogs using a clean wrapper pattern (to prevent windows.h macro collisions with Raylib) for native OS folder browsing right from the Hub.
- **Virtual File System (VFS):** Built a custom me::vfs subsystem. The engine now seamlessly maps virtual prefixes (engine:// for editor UI, game:// for user projects) to absolute physical paths, completely isolating internal engine assets from user game data.

### Asset Pipeline & Components
- **3D Asset Manager:** Expanded the Asset Manager to natively support loading and caching .glb and .obj files into Raylib Model structs. Implemented reference counting to share memory across identical instantiated models and properly unload them on engine shutdown.
- **Component Symmetry Refactor:** Renamed rendering components to clearly separate procedural math shapes from disk-loaded assets: Shape2DComponent, Shape3DComponent, Sprite2DComponent, and Model3DComponent.
- **Inspector Blindspot Fixed:** Upgraded the ImGui Inspector to dynamically recognize and edit properties for the newly added procedural shapes and 3D models.

### Editor Features & UX
- **Play / Stop Mode:** Implemented a non-destructive live-testing workflow. Clicking "Play" saves a pristine .temp_play.json snapshot and allows Lua scripts to manipulate the ECS. Clicking "Stop" freezes time and instantly restores the original pre-play ECS state.
- **Content Browser Panel:** Built an interactive, grid-based file explorer. Features smart color-coding based on file extensions and double-click interactions (double-clicking a .glb automatically spawns it at the origin; double-clicking a .json instantly loads the level).
- **Panel System Architecture:** Eliminated the "God Function" anti-pattern in on_render(). Extracted the ImGui UI into standalone, isolated classes (SceneHierarchyPanel, ContentBrowserPanel) that communicate with the core Editor App via std::function callbacks.
- **Professional Theming: O**verhauled the raw ImGui default style with a customized, rounded dark theme featuring custom UI accents and layout saving/loading via the Menu Bar.
- **Safe Scene Creation:** Replaced immediate scene wipes with a blocking ImGui Popup Modal for creating new scenes, ensuring files are properly named and automatically saved into the VFS before clearing the active world.

## [0.7.0] - 2026-05-10
Huge scope change. Mini Engine Raylib is now not an engine as a library, but its own engine executable with a GUI editor.

### Architecture & Scope
- **Standalone Editor:** Replaced the C++ Sandbox approach with a dedicated `EditorApp`. The engine is now driven by a GUI layer acting as a toolset over the core logic.
- **Data-Driven Scenes:** Completely removed C++ `Scene` class inheritance (e.g., `TestScene`). Implemented a purely functional `scene_manager` that constructs levels dynamically from JSON files directly into the ECS.
- **Engine Loop Split:** Moved core system updates (like `script_update`) directly into the `engine.cpp` main loop, while leaving rendering logic flexible for the application/editor to control.

### Editor Features
- **ImGui Viewport:** The game now renders into a dynamically resizing `RenderTexture2D` embedded within a Dear ImGui "Scene View" window, rather than full-screen.
- **Hierarchy & Inspector:** Added UI windows to list all ECS entities and live-edit their internal memory (Position, Rotation, Scale) using ImGui drag sliders.
- **Decoupled Editor Camera:** Extracted the free-fly camera out of the game's ECS. The Editor now uses its own private components, ensuring "Save Scene" doesn't write editor tools into the game data.
- **Input Routing:** Game inputs are intelligently blocked when ImGui has focus. The Editor Camera is activated (and the mouse cursor locked/hidden) only while holding Right-Click over the Scene View.
- **Main Menu Bar:** Added File menu options for "New Scene", "Save Scene", and "Load Default Scene", with auto-population of default primitives on start.

### Engine & ECS
- **Tag Component:** Added `me::components::TagComponent` to give entities human-readable names in the UI.
- **Serialization Expansion:** Expanded JSON `save()` and `load()` to support `TagComponent`, `ScriptComponent`, and `MeshRendererComponent` (resolving narrowing cast issues with Raylib colors).
- **Script Caching:** Optimized the `ScriptSystem` to cache Lua `update` function pointers upon initialization, preventing severe performance drops from string-based lookups every frame.
- **Memory Safety:** Transitioned the Lua `sol::state` manager from raw pointers to `std::unique_ptr` to guarantee clean destruction.

### Dependency Management
- **ImGui Vendoring:** Dropped `vcpkg` for ImGui. Switched to a manual vendor approach (`vendor/imgui`) alongside `rlImGui` to guarantee internal header (`imconfig.h`) compatibility.

## [0.6.0] - 2026-05-07
### Added
- **Lua Scripting Support:** Lua can now be used to give costum scripts to an entity.

## [0.5.1] - 2026-04-25
### Added
- **Scene Systems:** `me::Scene` now natively manages user-defined ECS systems. Added the `add_system<T>(Args&&...)` template method to easily instantiate and attach gameplay systems to a specific scene.

### Changed
- **System Architecture:** Refactored `me::System` to align with a purer ECS philosophy. Systems no longer hold a stateful reference to the `Registry` in their constructor.
- **System API:** The `on_update` signature was updated to receive the registry dynamically: `virtual void on_update(Registry& registry, float dt) = 0`.
- **Scene Execution:** `me::Scene::on_update(float dt)` now automatically fetches the global registry and iterates through all attached user systems, executing them in the order they were registered.

### Removed
- **System Render:** Removed `on_render()` from the base `me::System` interface to strictly enforce the engine's pipeline rules (Simulation/Logic runs first, Rendering is handled automatically by the engine at the end of the frame).

## [0.5.0] - 2026-04-25
Mini Engine Raylib went through a huge code refactor and overhaul.

## [0.4.1] - 2026-02-01
### 3D Transformation
- **Core Pivot:** The engine is now natively 3D.
- **Transform:** Replaced `Transform2D` with `Transform`. Now supports `x, y, z`, Euler rotation (`rotX, rotY, rotZ`), and 3D scale.
- **Rendering:** Removed `Render2D` and `SpriteRenderer`. Added `Render3D` and `MeshRendererComponent` (supporting `Cube`, `Sphere`, and `Plane` primitives).

### Added
- **Camera System:** Added `me::camera::UpdateFreeFly()` for decoupled FPS-style camera movement (WASD moves flatly, Mouse looks freely).
- **Camera Helpers:** Added `me::camera::LookAt()` to automatically calculate Pitch and Yaw to face a target position.
- **Input Defaults:** Updated `InputDefaults` to support 3D axes (`MoveZ` on W/S, `MoveY` on Q/E).

### Changed
- **Serialization:** Updated `Scene::Save` and `Scene::Load` to handle the new 3D component structures (JSON format updated).
- **Entity Creation:** `CreateEntity` now attaches a 3D `Transform` by default.

### Removed
- **2D Systems:** Temporarily removed `Physics2D`, `Animation`, and `DebugDraw` from the build pipeline to facilitate the 3D transition.

## [0.4.0] - 2026-01-28
### Architectual Refactor
- **Namespace Organization:** Moved core systems into dedicated namespaces (`me::physics`,  `me::lifetime`,  `me::animation`) to replace the generic `me::systems` prefix.
- **Decoupling:** Removed gameplay-specific components (`Projectile`,  `Hittable`,  `Health`) from the core engine. These are now implemented as user-defined components in the Sandbox to demonstrate engine extensibility.
- **Cleanup:** Deleted `ProjectileSystem` source and headers from the engine core.

### Added
- **Debug Colors:** Added `me::Color` fields to `AabbCollider` and `CircleCollider` components, allowing per-entity debug visualization colors.
- **Color API:** Added `ToHex()` and `ToHexRGB()` helper methods to `me::Color`.
- **Sandbox Implementation:** Implemented a robust "Game Loop" pattern in the Sandbox (`Input` -> `Logic` -> `Physics` -> `Reaction` -> `Render`) and added custom `Health` and `Projectile` logic.

### Changed
-   **Debug Drawing:** Updated `DebugDraw` to internally handle `me::Color` to Raylib conversion, preserving the engine's abstraction layer.
-   **System API:** Renamed system update functions to a standardized `Update(dt)` (e.g.,  `me::physics::Update`).

## [0.3.0] - 2026-01-19
### Architectual Overhaul
- **Pure ECS Transition:** Refactored the core Engine to use a generic Registry with `Pool<T>`. Components are no longer hardcoded in the engine maps.
- **System Architecture:** Logic moved from Scene/Object classes into pure Systems (`ProjectileSystem`, `LifetimeSystem`, `CameraFollowSystem`, etc.).
- **Entity API:** Updated `Entity` wrapper to use template methods (`Add<T>`, `Get<T>`) for cleaner, type-safe component access.

### Added
- **New Components:** `Lifetime`, `Health`, `Hittable`, `Projectile`, and `AsteroidData` (example).
- **Scene Customization:** Added `virtual me::Color GetClearColor()` to `GameScene`, allowing scenes to control their own background color dynamically.
- **Scene Management:** Added `me::scene::manager::UnloadCurrent()` to ensure clean shutdown when quitting or transitioning.
- **Collision Debugging:** Updated DebugDraw to iterate generic pools, allowing visualization of any collider type automatically.

### Changed
- **Sandbox Reset:** Completely cleared the sandbox project to provide a clean "TestBed" template for new users, removing the hardcoded Asteroids game logic.
- **Camera Logic:** `Camera2D` component no longer holds position/rotation. The camera now uses the standard `Transform2D` component for movement, unifying it with other entities.
- **Rendering:** `Render2D` now correctly reads the active camera's `Transform2D` for view calculation.

### Removed
- **Hardcoded Logic:** Deleted `LaserProjectile`, `Asteroid`, and `Player` classes from the engine core. These are now implemented via Systems and Components in the game layer.
- **Legacy Helpers:** Removed old `Attach` camera functions in favor of standard ECS parenting/following.

## [0.2.0] - 2025-11-05
### Added
- `GameApp` class and `me::Run(...)` helper to own the main loop and lifecycle.
- Scene orchestration layer with `GameScene` interface (`OnEnter`, `OnExit`, `OnUpdate`) and `me::scene::manager` for named scene switching.
- Automatic scene file creation under `/scenes/` when registering scenes, including a default active `Camera2D` entity.
- Entity naming and lookup helpers (`SetName`, `GetName`, `FindEntity`) for easier game-side logic.
- `GameScene::SaveSelf()` convenience for saving the current world back to its scene file.
- Improved example game setup using `MyGame : GameApp` with `Base` and `Space` scenes.

### Changed
- Centralized component storage into a shared `Registry`, simplifying ECS internals and iteration.
- Scene loading/saving now consistently targets the `/scenes/<filename>.json` directory.
- SpriteRenderer removal now releases texture references correctly, improving asset lifetime management.

### Fixed
- Potential texture leaks when destroying entities with sprite components.
- Various small stability and API consistency issues revealed while wiring real game flow.

## [0.1.0] - 2025-10-18
### Added
- Initial public release of MiniEngine  
- Core ECS system and entity management (Entity, Components, Registry)  
- 2D Rendering with sprite layers and cameras  
- Scene serialization to JSON  
- Input system with digital axes and actions  
- Simple AABB physics and velocity integration  
- Asset management with reference counting  
- Basic animation system  
- Camera follow and multiple viewport support  
- Skeleton game loop and sample scene
