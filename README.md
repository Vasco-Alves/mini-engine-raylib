# Mini Engine Raylib

**A small scene engine built on [raylib](https://www.raylib.com/). Build a scene once — press Play to run it as a game, or press Render to path-trace it into stills and animations.**

> Version 1.1.0 · C++23 · MIT · Windows (primary), Linux (builds, lightly tested), macOS (experimental: CPU path tracing only — Apple's OpenGL has no compute shaders)

![Editor](docs/images/editor_hero.png)

One scene description, many consumers: the ECS is the single source of truth, and the real-time rasterizer, the path tracer, the physics simulation, the scripting runtime and the animation system are all just different systems reading the same scene.

## The three modes

| Mode | What it is | Frame pacing |
| --- | --- | --- |
| **EDIT** | Arrange the scene: gizmos, inspector, hierarchy, undo/redo | capped to monitor refresh |
| **PLAY** *(green)* | The scene as a game: Jolt physics, Lua scripts, 3D audio | capped to monitor refresh |
| **RENDER** *(purple)* | The scene as film: progressive CPU/GPU path tracing | uncapped — every frame is a sample |

Switching is one click on the toolbar; the viewport frame, toolbar badge and window title show which mode you're in, and Edit and Render each remember their own panel layout (`Layout > Per-Mode Layouts`).

## Features

### Engine core

- **ECS** — entities are plain data; systems iterate component pools ([mini-ecs](vendor/mini-ecs)).
- **Component registry** — one registration per component drives scene serialization, duplication, native-handle cleanup, undoable add/remove *and* animation.
- **Scene serialization** — scenes are JSON; projects have their own asset folders.
- **Lua scripting** — per-entity scripts with `start`/`update` callbacks and hot-reload ([sol3](vendor/sol3) + Lua).
- **Physics** — rigid bodies and box/sphere colliders via [Jolt](vendor/joltphysics), with collision callbacks into Lua, trigger volumes, and raycasts.
- **Audio** — sound effects and streaming music with simple 3D spatialization.

### Game side

- Real-time forward renderer with a custom lighting shader (point + directional lights) and **directional-light shadow maps** (PCF-filtered, camera-following, per-light toggle).
- Play mode runs the full simulation loop (scripts → physics → transforms) with pause and single-step controls.

### Render side

- **Path tracer** (CPU and GPU compute backends with matching features): next-event estimation, soft shadows from area lights, inverse-square falloff, glass with Fresnel + Beer–Lambert tinting (with per-material tint strength), depth of field with click-to-focus, exposure, firefly clamping, art-directable sky, ACES tonemapping, progressive accumulation.
- **RT Play** — flip the toolbar's "RT" toggle and Play mode renders through the path tracer in real time, built for retro/pixelated games: low internal resolution buys the per-frame samples, and a locked noise pattern turns residual grain into stable dithering. Export Game can ship the same renderer (`Raytraced renderer` checkbox).
- **Feature toggles + quality presets** (Full / Lite / Flat) — turn off the *stochastic* features (indirect GI, soft shadows) and the image is noise-free at one sample per pixel, mirrors and sharp glass still intact, running at full framerate. The Flat preset (direct light + hard shadows) is a crisp, zero-noise renderer ideal for a simple game; presets and toggles ship with raytraced exports.
- **Watchdog-safe GPU dispatch** — large frames render in row bands so heavy exports can't trigger an OS driver reset.
- **PNG export** — its own resolution/samples/bounces, independent of the viewport, with progress and cancel.
- **Animation** — keyframe the camera (position, look-at, FOV, exposure, focus) *and* entity components (Transform, Material, lights, shapes); scrub, preview in the viewport, then render an image sequence ready for `ffmpeg`.

### Editor

- Dockable ImGui UI: hierarchy (drag-drop parenting, inline rename), inspector, content browser, console, animation timeline, stats overlay.
- **Undo/redo for everything** — field edits, add/remove component, create/delete/ duplicate entity.
- Scene management with unsaved-changes guard, Open/Save As, and a dirty marker in the title bar.

## Repository layout

```
engine/     Static library `engine` — the runtime (no editor/UI dependencies)
  include/mini-engine-raylib/   Public headers (the engine's API surface)
  src/                          Implementation
  assets/shaders/               Engine runtime shaders (lighting + raytracer compute),
                                copied next to any engine front-end's executable
editor/     Executable `editor` — the ImGui tool built on top of `engine`
game/       Executable `game` — the shipped game runtime (no editor, no ImGui);
            "File > Export Game..." packages it with a project into a standalone game
docs/       Documentation (see ARCHITECTURE.md)
tests/      Headless tests (scene round-trip) + dev tools (shader compile check)
vendor/     Third-party dependencies (raylib, imgui, lua, sol3, jolt, json, mini-ecs, …)
```

The engine is intentionally decoupled from the editor: `engine` knows nothing about ImGui.
The editor and the game runtime are two front-ends over the same engine API.

## Building

> **Just want to try it?** Grab a prebuilt zip from the [Releases](../../releases) page — no compiler needed. Building from source is for contributors, forks, and platforms without a prebuilt download.

Requires a C++23 compiler, CMake ≥ 3.5 (3.21+ recommended), and a GPU/driver with **OpenGL 4.3** (raylib requests a 4.3 context at startup; the GPU path-tracer backend uses compute shaders). The build statically links the MSVC runtime and pulls every dependency from `vendor/` via `add_subdirectory`, so make sure the submodules are present.

```bash
git clone --recursive https://github.com/Vasco-Alves/mini-engine-raylib.git
cd mini-engine-raylib
# (if you already cloned without submodules: git submodule update --init --recursive)
```

### With CMake presets (recommended)

```bash
cmake --preset x64-debug        # or: x64-release, linux-debug
cmake --build --preset x64-debug
```

Presets use the Ninja generator. The editor builds as **`MiniEngine`** (plus the `game` runtime) in `out/build/<preset>/bin/`, with `assets/` copied next to it automatically.

> Tip: CPU-backend path tracing is **10–30× faster** in a Release build — use `x64-release` for final CPU renders.

### Tests

A headless serialization round-trip test builds as the `scene_roundtrip_test` target (and is registered with CTest):

```bash
cmake --build build --target scene_roundtrip_test
ctest --test-dir build -C Debug          # or just run the scene_roundtrip_test executable
```

## Running

Launch `MiniEngine`. On first start you get the **Project Hub** — create a new project (which scaffolds `assets/{scenes,scripts,models,textures,audio}` under the chosen folder) or open a recent one.

**New projects start with the demo scene**: glass and mirror spheres, a gold dragon model, a glowing cube, a roughness lineup — and a physics cube with a Lua script and a sound attached. Press **Play** and hit **SPACE** to make it jump; press **RENDER** to path-trace the same scene; open the **Animation** panel for a ready-made camera move + glow ramp to try **Render Animation** with. (`Ctrl+N` still creates a clean minimal scene.)

### Editor controls

| Action | Input |
| --- | --- |
| Fly camera | Hold **RMB** in the viewport, **WASD** + **Q/E** up/down |
| Camera sprint / precision | **Shift** / **Ctrl** while flying; **scroll** tunes base speed |
| Orbit camera | Hold **RMB** + **Alt** |
| Focus selected | **F** |
| Gizmo: none / move / rotate / scale | **Q / W / R / S** (hold **Ctrl** to snap) |
| Select | Left-click an object in the viewport |
| Rename | **F2** or double-click in the hierarchy |
| Duplicate / delete | **Ctrl+D** / **Delete** |
| Undo / redo | **Ctrl+Z** / **Ctrl+Y** (or **Ctrl+Shift+Z**) |
| Save / Save As / new scene | **Ctrl+S** / **Ctrl+Shift+S** / **Ctrl+N** |
| Play / stop | **Ctrl+P** (or the toolbar) |
| Fine-tune any drag field | Hold **Alt** (10× finer); **Ctrl+click** to type a value |

### Rendering stills and animations

1. Click **RENDER** on the toolbar — the path tracer takes over the viewport and accumulates progressively (click any object to set the depth-of-field focus).
2. **Export to PNG** in Raytracer Settings renders at its own resolution/samples without touching your viewport setup.
3. The **Animation** panel keyframes the camera and entity components: capture keys, scrub to check, then **Render Animation…** writes `renders/anim_<timestamp>/frame_0001.png …` Join the frames with the `ffmpeg` one-liner printed in the console, e.g.:

```bash
ffmpeg -framerate 30 -i frame_%04d.png -c:v libx264 -pix_fmt yuv420p out.mp4
```

Animations are saved next to the scene as `<scene>.anim.json` and load with it.

### Exporting your game

**File > Export Game...** packages a standalone game — Godot-style, no compiler involved: the generic `game` runtime (compiled together with the engine) is copied and renamed, the engine shaders and your project's assets are copied next to it, and a `game_config.json` records the window settings and which scene to boot. The result is a folder you can zip and send to anyone:

```
MyGame/
  MyGame.exe          the game runtime
  game_config.json    window settings + boot scene
  assets/shaders/     engine shaders
  data/               your project's content (scenes, scripts, models, audio)
```

The exported game starts straight in play mode: physics, Lua scripts and audio run, and it renders from the scene's active Camera component. Build with `x64-release` before exporting for the fast runtime.

## Scripting

Attach a `.lua` file to an entity (drag it onto the Inspector's script drop-zone). Scripts define optional `start` and `update` functions and run while the scene is **playing**:

```lua
function start(self)
    -- self is the Entity this script is attached to
    print("spawned at", self:get_transform().position.x)
end

function update(self, dt)
    if Input.action_down("MoveRight") then
        self:set_velocity(5, 0, 0)            -- requires a RigidBody
    end
    if Input.action_pressed("Jump") then
        local fx = Scene.spawn(Scene.find("ExplosionTemplate")) -- prefab-style spawning
        fx:teleport(0, 2, 0)
        fx:play_sound()
    end
    if self:get_transform().position.y < -50 then
        Scene.load("game://scenes/game_over.json")  -- switch scenes (menus, levels, ...)
    end
end
```

Scripts can also define `on_collision_enter(self, other)` / `on_collision_exit(self, other)`, which fire after the physics step (mark a RigidBody **Is Trigger** for sensor volumes). The API covers entity lookup/spawning/destruction, transforms with full `Vector3` math, materials and lights, the camera, physics velocities/impulses/raycasts, sounds and music, scene switching and `Engine.quit()` — see the **[Lua scripting reference](docs/LUA_API.md)**. Saving the file hot-reloads it in play mode.

## Architecture

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the layering, the per-frame system order, the serialization model, and a checklist for adding a new component type.

## Vendored dependencies

- [**mini-ecs**](https://github.com/Vasco-Alves/mini-ecs) — a custom-built, lightweight Entity-Component-System engineered for contiguous component pools and fast iteration.
- [**raylib**](https://github.com/raysan5/raylib) — windowing, input, and the rendering context.
- [**Dear ImGui**](https://github.com/ocornut/imgui) + **ImGuizmo** (via `rlImGui`) — the entire editor UI.
- [**Jolt Physics**](https://github.com/jrouwe/JoltPhysics) — multi-threaded 3D collision and rigid-body simulation.
- [**sol3**](https://github.com/ThePhD/sol2) + **Lua** — the scripting environment bridge.
- [**nlohmann_json**](https://github.com/nlohmann/json) — scene, animation and config serialization.
- [**portable-file-dialogs**](https://github.com/samhocevar/portable-file-dialogs) — native OS folder/file dialogs.

## Making a release zip

```bash
cmake --build --preset x64-release
cd out/build/x64-release && cpack -G ZIP -C Release
```

This produces `mini-engine-raylib-<version>-win64.zip` containing the editor, the game runtime, the assets and the docs — a standalone, redistributable build.

## Roadmap / known limitations

- **Animation** — entity keys hold whole-component snapshots (edit them via "Update From Scene"); entity tracks re-bind by entity *name* across scene loads.
- **No 2D rendering** — the engine is 3D-focused; the early 2D scaffolding was removed and can return later (a component is one registry entry + one inspector row).
- **Adding a component type is one registration per side** — an entry in `engine/src/ecs/component_registry.cpp` and a row in the editor's `inspector_components()` table.

## License

The engine is released under the [MIT License](LICENSE). Vendored dependencies retain their own licenses (see each folder under `vendor/`).
