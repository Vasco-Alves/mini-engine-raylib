# Changelog

All notable changes to this project will be documented in this file.

## [1.0.0] - 2026-06-12

The first stable release. The identity is complete and shipping: *build a scene once —
press Play to run it as a game, or press Render to path-trace it into stills and
animations.* Both halves produce standalone artifacts (exported games, rendered image
sequences), the editor is undo-safe end to end, and the whole thing packages into a
redistributable zip.

### Added

- **Demo project**: new projects are seeded with a showcase scene — glass and mirror
  spheres, a gold dragon model, an emissive cube, a roughness lineup, and a physics cube
  with a Lua jump script + sound (press Play, hit SPACE). A sample animation sidecar
  (camera dolly + emission ramp) is included, ready for Render Animation. `Ctrl+N` still
  creates a minimal clean scene.
- **App icon**: the editor and game executables carry the engine icon (play triangle +
  render orb), and the editor sets it as the window/taskbar icon at runtime.
- **Help menu**: a Controls cheat-sheet window (every shortcut) and the About dialog.
- **Cross-platform groundwork**: macOS caps OpenGL at 4.1, so the build now selects the
  GL version per platform (the GPU path-tracer backend is unavailable on Apple platforms;
  the raytracer detects this at runtime and the CPU backend keeps working). AVX2 is only
  requested on x86-64, unblocking ARM builds.

### Changed

- **The editor ships as `MiniEngine.exe`** (the product name) instead of `editor.exe`.
- A missing game runtime at export now explains the fix for packaged installs, not just
  source builds.

### Fixed

- **Transform hierarchy cycle guard**: a corrupt or hand-edited scene file containing a
  parent cycle no longer overflows the stack — the loop is broken with a warning, cycle
  "islands" still update standalone, and entities whose parent id doesn't resolve are
  treated as roots instead of freezing forever.
- **Scene loading resilience**: a malformed component is logged and skipped instead of
  aborting the whole scene load.
- **The last non-undoable edits**: hierarchy drag-drop reparenting (including Unparent)
  and Lua script attach/detach now go through the command history.
- New headless `animation_eval` test suite pins down keyframe interpolation, easing,
  clamping and JSON-lerp semantics (registered with CTest).

## [0.17.0] - 2026-06-11

The release-readiness update: the engine's second front-end (the game runtime) ships,
and the project gains the release hygiene expected of a 1.0 candidate.

### Added

- **Game runtime (`game/`)**: a standalone `me::Application` front-end with no editor and
  no ImGui. It reads `game_config.json`, mounts the packaged assets, loads the boot scene
  and runs it in play mode — physics, Lua scripts and 3D audio — rendered from the scene's
  active `CameraComponent` (with a safe fallback view when a scene has no camera).
- **File > Export Game…**: packages a standalone game Godot-style — the prebuilt runtime is
  copied and renamed, engine shaders and the project's assets are copied next to it, and a
  `game_config.json` records window settings and the boot scene. Native folder picker,
  boot-scene selector, no compilation at export time.
- **LICENSE**: the project is now formally MIT licensed.
- **Versioning**: single-source version constant (`core/version.hpp`), a Help > About
  dialog in the editor, and this changelog brought back into service.
- **Scene format version field**: scene files now carry `"version"`, so future format
  changes can be migrated (or at least warned about) instead of silently misread.
- **Packaging**: CMake install rules + CPack produce a redistributable zip
  (editor + game runtime + assets + docs) via `cpack -G ZIP`.

## [0.16.0] - 2026-06-11

The identity update: the engine now states — and shows — what it is: *build a scene once;
press Play to run it as a game, or press Render to path-trace it into film.*

### Added

- **Per-mode workspaces**: Edit and Render modes each remember their own panel set
  (View > Panels) and dock layout, swapping automatically on mode change. Render mode
  drops the content browser/console and brings in the animation timeline and raytracer
  settings.
- **Mode visual identity**: color-coded viewport frame (green = playing, purple =
  rendering), a toolbar mode badge, and the mode in the window title.
- **Shipped default layouts**: fresh installs start with the layouts under
  `editor/assets/layouts/` (exported via Layout > Save As Shipped Default), so the first
  run looks like the author's setup.
- **Explicit-save layout model**: ImGui's continuous ini autosave is disabled; layouts
  persist only when saved (Layout > Save Layout), with in-session memory across mode
  switches and a Reset to Default.

### Changed

- **Asset ownership**: the runtime shaders (lighting + raytracer compute) moved from
  `editor/assets/` to `engine/assets/` — they were always loaded through the `engine://`
  mount and belong to the engine, not the editor.
- The editor's offline-render flows (PNG export + animation render) moved into their own
  translation unit (`editor_offline_render.cpp`).
- README/ARCHITECTURE rewritten around the one-scene-many-consumers identity.

## [0.15.0] - 2026-06-11

The animation update: keyframe the camera, the environment and entity components, then
render the result as an image sequence.

### Added

- **Keyframe animation**: camera track (position, look-at, FOV, exposure, aperture,
  focus distance), environment track (sky horizon/zenith colors, sky intensity, ambient —
  sunsets are two keys), and entity tracks that animate whole components (Transform,
  Material, shapes, lights) through the component registry — captured as JSON snapshots
  with every numeric field interpolated, zero per-component animation code.
- **Timeline panel**: a real timeline with a time ruler, scrubbing playhead, draggable
  keyframe diamonds, per-key easing (linear/smooth), right-click context menus,
  double-click to jump, Ctrl+scroll zoom with pinned track labels, and direct value
  editing for camera/environment keys. All keyframe edits are undoable.
- **Animation preview**: plays in the Edit viewport in real time, and inside the path
  tracer in Render mode (one progressive sample per frame).
- **Offline animation render**: renders frame-by-frame at export resolution/samples into
  `renders/anim_<timestamp>/frame_0001.png …` with per-frame and per-sample progress,
  cancel support, and the ffmpeg one-liner logged for joining into a video.
- **Animation persistence**: tracks save to a `<scene>.anim.json` sidecar with the scene;
  entity tracks re-bind by entity name across scene loads.

## [0.14.0] - 2026-06-10

The editor quality-of-life update: undo for everything, real scene management, and an
editor that no longer burns the GPU while idle.

### Added

- **Structural undo/redo**: adding/removing components, creating/deleting/duplicating
  entities — all undoable. Deleting an entity snapshots every component plus hierarchy
  links; undo restores it and re-claims its children.
- **Scene management**: File > Open Scene, Save Scene As (Ctrl+Shift+S), an
  unsaved-changes guard on Exit/New/Open, and a dirty marker (`*`) in the title bar.
- **Hierarchy polish**: inline rename (F2 / double-click), Duplicate and Rename in the
  context menu, and an Edit menu with Undo/Redo/Duplicate/Delete.
- **Inspector precision**: hold Alt for 10x finer drag control on every field; gizmo
  snapping with Ctrl was joined by camera speed modifiers (Shift = sprint, Ctrl =
  precision) and scroll-to-tune fly speed with an on-screen toast.
- **Stats overlay** (View > Stats Overlay): FPS/frame time, entity count, and raytracer
  resolution/backend/sample progress in Render mode.

### Changed

- **Frame pacing**: the editor was running uncapped; Edit/Play now cap to the monitor
  refresh rate and Render mode uncaps (every frame is a path-tracer sample).
- Scene loads route through one guarded path; opening a scene from the content browser
  previously left Ctrl+S pointed at the *old* scene file (a data-loss bug) and kept a
  stale undo history that could resurrect entities from the previous scene.
- Component removal now releases native handles (models/audio) through the component
  registry, fixing leaks when removing asset-backed components.
- All non-vendor compiler warnings fixed; builds are warning-clean.

## [0.13.0] - 2026-06-10

The path tracer realism update, plus a hardened offline export pipeline.

### Added

- **Soft shadows**: point lights gained a radius and directional lights an angular size;
  shadow rays sample the light's disk/cone, so shadows soften with distance.
- **Physical glass**: Fresnel (Schlick) reflectance at interfaces and Beer–Lambert
  absorption through the interior — tinted glass darkens with thickness and its cast
  light takes the glass color. A per-material **Tint Strength** scales the absorption
  density (0 = always clear). Inverse-square falloff for point lights.
- **Depth of field**: thin-lens aperture + focus distance with **click-to-focus** in the
  render view; exposure control; **firefly clamp** to suppress bright noise specks.
- **Camera component panel** in the inspector, and play mode now renders from the scene's
  active camera instead of the editor fly-cam.
- **Export estimates**: the export dialog shows MP/sample, approximate VRAM and workload
  warnings, recommending a backend when settings are heavy.

### Fixed

- **GPU driver resets on heavy exports (TDR)**: the compute dispatch is now split into
  row bands sized against the bounce count, so no single dispatch can outlive the OS GPU
  watchdog. High-resolution GPU exports no longer crash the app.
- **Analytic cube interior hits**: rays starting inside a cube (refraction) reported no
  exit hit, so glass cubes never tinted — both backends now fall back to the exit face,
  matching the sphere's behavior.
- **Depth-of-field lens sampling** was per-frame instead of per-pixel, making the whole
  image lurch while accumulating instead of blurring.
- **Export correctness**: exports previously rendered at viewport resolution regardless of
  the dialog settings (the viewport size tracker fought the export resize), and the
  viewport pass kept rendering during exports. Offline renders now own the raytracer,
  pause the viewport, restore every overridden setting, and support cancel.

## [0.12.0] - 2026-06-09

The architecture update: one registration per component now drives everything, and the
GPU path tracer becomes the default backend.

### Added

- **Component registry** (`me::ecs`): a single per-component registration providing
  has/save/load/clone/remove/on-destroy operations. Scene serialization, entity
  duplication, native-handle cleanup — and later undo and animation — all derive from it.
- **GPU compute path tracer** as the default backend, mirroring the CPU feature set
  (BVH/BLAS traversal, NEE direct lighting, progressive accumulation, ACES, Halton AA),
  with shared scene flattening into SSBOs.
- **Headless tests**: a scene save→load→save round-trip test (CTest) and a compute-shader
  compile check used to validate GLSL changes without driving the editor.

### Changed

- Scene save/load rewritten on top of the registry with a three-pass load (create + id
  map, load components, relink hierarchy).
- One canonical per-frame order (`world_update`): scripts → physics → transforms,
  removing a one-frame physics lag.
- The inspector was rebuilt around a component table (single source of truth for panels
  and the Add Component menu) with RAII section headers fixing a latent ImGui tree-stack
  imbalance.
- OpenGL 4.3 is requested through raylib's own configuration knob instead of a leaky
  global define.

### Removed

- The dormant 2D component scaffolding (Shape2D/Camera2D/Sprite) — the engine is
  3D-focused; 2D can return as ordinary registry entries when wanted.

### Fixed

- `me::audio::update()` was never pumped, so streamed music stalled after a few seconds.
- Entity duplication and deletion now go through the registry (duplicating an entity
  previously aliased the source's children list; deletion leaked native handles).

## [0.11.1] - 2026-06-08

### Added:

- **Glass & Refraction**: Introduced `transmission` and `ior` (Index of Refraction) to `MaterialComponent`, allowing for realistic rendering of transparent materials like glass, water, and diamonds.
- **Physical Light Support**: Upgraded Point and Directional light intensity calculations to work in Linear Space (sRGB-to-Linear conversion), resulting in more vibrant and physically accurate lighting.
- **ACES Filmic Tonemapping**: Implemented ACES curve for HDR-to-LDR mapping, fixing blown-out highlights and color desaturation ("washed-out" look).
- **QMC Anti-Aliasing**: Integrated Halton sequence for camera ray jittering, significantly improving edge quality and pixel distribution.
- **Fast RNG**: Replaced standard library random with a high-performance PCG hash for bounce rays, significantly reducing noise and improving performance.

### Changed:

- **BLAS Acceleration**: Upgraded `TriangleBVH` from a brute-force list to a structured BVH tree, enabling real-time raytracing for high-poly 3D models.
- **Raytracer Pipeline**: Refactored the raytracing pipeline to use physically-based material properties (Linear space conversion) and refined the integration logic to prevent double-gamma errors.
- **Renderer Stability**: Fixed a bug where primitive shapes in the OpenGL editor would inherit material uniforms from subsequently drawn 3D models.

## [0.11.0] - 2026-05-08

### Added:

- **Raytracing System**: Initial implementation of the CPU-based path tracer.
- **Render Mode**: Introduced a "RENDER" button to toggle between the rasterized editor view and the path-traced viewport.
- **BVH System**: Built a Two-Level Acceleration Structure (TLAS/BLAS) to support real-time raytracing of primitives (`Sphere`, `Plane`, `Cube`) and custom 3D models (`Model3DComponent`).
- **Export**: Added functionality to export the accumulated raytraced result to a high-quality PNG file.

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
