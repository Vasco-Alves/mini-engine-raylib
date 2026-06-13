# Architecture

This document describes how `mini-engine-raylib` is put together: the layering, the per-frame update order, how scenes are serialized, and how to extend the engine. It supersedes the older sketch in `system_order_updates.txt`, whose namespace/function names (`me::scene::manager::Update`, `me::physics::Step`, `me::lifetime`, `me::animation`, …) no longer match the code.

## Identity: one scene, many consumers

Mini Engine Raylib is a **scene engine with two outputs**: the same scene plays as a game (Play mode: Jolt physics, Lua scripts, audio) or renders as film (Render mode: progressive CPU/GPU path tracing, PNG export, keyframe animation). This is not two products sharing a repo — the ECS + component registry is the single source of truth, and the rasterizer, the path tracer, the simulation systems, the serializer and the animation system are all just different consumers of the same component data. Design decisions should preserve that: new scene-affecting features belong in components + registry entries, so every consumer (and the editor's undo, duplication and animation) picks them up for free.

Engine runtime assets (the lighting shaders and the raytracer compute shader) live in `engine/assets/` and are copied next to whichever front-end executable is built — the editor and the game runtime (`game/`). The game runtime is a ~170-line `me::Application` that mounts the packaged assets, loads the boot scene from `game_config.json` and runs itin play mode; the editor's "File > Export Game..." packages it with a project (copy-only, Godot-style — nothing is compiled at export time). The editor's offline-render flows (PNG export + animation render) live in `editor/src/core/editor_offline_render.cpp`, separate from the main `editor_app.cpp`.

## Layers

```
   ┌──────────────────────────────┐  ┌──────────────────────────────┐
   │  editor (MiniEngine.exe)     │  │  game (game.exe)             │   executables
   │  ImGui tool, panels, gizmos  │  │  the shipped runtime,        │
   │  timeline, offline renders   │  │  no editor / no ImGui        │
   └───────────────┬──────────────┘  └────────────────┬─────────────┘
                   │ depends on                       │
        ┌──────────▼──────────────────────────────────▼──────────┐
        │  engine  (me::*)                                       │   static library
        │  core · ecs · systems · render · scene ·               │
        │  scripting · audio · assets · input                    │
        └───────────────────────┬────────────────────────────────┘
                                │ depends on
        ┌───────────────────────▼────────────────────────────────┐
        │  vendor  (raylib, jolt, lua/sol3, imgui,               │
        │  mini-ecs, nlohmann_json)                              │
        └────────────────────────────────────────────────────────┘
```

The key boundary: **`engine` has no dependency on ImGui or the editor.** Both executables are front-ends built on the engine's public API (`engine/include/mini-engine-raylib/...`): the editor subclasses `me::Application` with the full tooling stack, and the game runtime ([`game/src/main.cpp`](../game/src/main.cpp)) does the same with only what a shipped game needs.

## Application & engine loop

The engine exposes a small free-function API (`me::init`, `me::run`, `me::get_registry`, play/pause/step controls) backed by a single static `EngineState` ([`engine.cpp`](../engine/src/core/engine.cpp)). A consumer subclasses [`me::Application`](../engine/include/mini-engine-raylib/core/application.hpp) and overrides `on_start` / `on_update` / `on_render` / `on_resize` / `on_shutdown`, then calls `me::run(app, config)`.

`me::run` owns the main loop. Each frame it:

1. polls input and updates window state,
2. calls `me::world_update(dt)` — the canonical simulation step: **scripts → physics → collision callbacks → transforms** (scripts and physics gated by play/pause/step; transforms always run),
3. calls `app.on_update(dt)`,
4. pumps streamed music (`me::audio::update()`),
5. brackets `app.on_render()` between `BeginDrawing` / `EndDrawing`,
6. processes deferred ECS deletions, releasing native handles (audio/model/texture) first,
7. applies a pending scene switch (Lua's `Scene.load` defers to here, the only point where no system is iterating the registry; mid-play the physics world is rebuilt around the new scene).

`world_update` runs physics *before* the transform pass, so a body moved by physics gets its world matrix rebuilt the same frame (no one-frame lag). The **spatial-audio** system and all **rendering/tooling** stay consumer-side — the editor calls them in `on_update` / `on_render`, since the audio listener and the viewport are front-end-specific.

### Per-frame order in the editor

```
me::run:
  input::poll
  world_update(dt):                            // shared simulation order
      (if playing) script_update -> physics::update   // logic sets velocities, Jolt integrates
      (if playing) script_dispatch_collisions          // on_collision_enter/exit from that step
      transform_update                                 // hierarchy → world matrices (post-physics)
  EditorApp::on_update(dt):
      file-drop import, editor camera fly/orbit
      animation preview playback (applies the evaluated track state)
      (if rendering) raytracer.on_update(...)  // paused while an offline export/render owns it
      poll_shortcuts()
      systems::audio_update(...)               // 3D pan/attenuation, triggers (front-end listener)
  audio::update()                              // pump music streams
  EditorApp::on_render():
      viewport: render::render_world(editor cam) + gizmo/light/physics debug draw
      ImGui: panels, toolbar, menus, modals
  registry.process_deletions(...)
```

The canonical simulation order now lives in `me::world_update`; the older `system_order_updates.txt` sketch predates it. Editor-only concerns (camera, tooling, spatial audio) layer on top in `on_update` / `on_render`.

## ECS model

[mini-ecs](../vendor/mini-ecs) is a small archetype-less ECS:

- **Entities** are `uint32_t` ids (`me::entity::entity_id`); `0` is the reserved `null` value and ids start at `1`. `me::Entity` is a lightweight `{id, Registry*}` handle with a convenience component API.
- **Components** are plain structs (see `engine/include/mini-engine-raylib/ecs/`). No inheritance, no registration.
- **Pools** are dense arrays. Systems iterate via `registry.view<T>()` and access the parallel `entity_map[i]` / `components[i]` arrays directly.
- **Deletion is deferred**: `destroy_entity` marks an entity, and `process_deletions` (end of frame) runs a pre-delete callback so native GPU/audio handles can be released before the row disappears.

### Transforms & the scene graph

`TransformComponent` carries position/rotation(euler)/scale, a quaternion (the rotation source of truth — the gizmo writes it directly to avoid Euler drift), a parent id + children list, a cached `model_matrix`, and a dirty flag. [`transform_system.cpp`](../engine/src/systems/transform_system.cpp) walks roots → children recursively, rebuilding only dirty subtrees and propagating dirtiness downward. The inspector edits Euler; the system syncs Euler→quat (or quat→Euler) depending on which changed.

The walk is hardened against corrupt scene files: a visited set breaks parent **cycles** (instead of overflowing the stack), entities whose parent id doesn't resolve are treated as roots, and cycle "islands" with no root still get updated standalone — so broken hierarchy data degrades to a warning, never a crash or an invisible entity.

## Editor: selection, gizmos, and undo

- **Selection** is broadcast via the `EventBus` (`EntitySelectedEvent`); the hierarchy panel holds the current selection. `me::entity::null` (`0`) is the "nothing selected" sentinel.
- **Gizmos** ([`viewport_panel.cpp`](../editor/src/panels/viewport_panel.cpp)) manipulate the world matrix via ImGuizmo, convert back to local space relative to the parent, and decompose into position/rotation/scale.
- **Undo/redo** covers every scene-mutating operation, through two command families:
  - *Field edits* ([`icommands.hpp`](../editor/include/editor/core/icommands.hpp)): `ModifyComponentCommand<T>` snapshots a component's whole value before/after an edit. The inspector captures `start_state` on `IsItemActivated` and pushes on `IsItemDeactivatedAfterEdit`; the gizmo does the same around a drag.
  - *Structural edits* ([`entity_commands.hpp`](../editor/include/editor/core/entity_commands.hpp)): add/remove component, create/delete/duplicate entity, reparenting, and script attach/detach — all powered by the component registry's JSON snapshots, so undo can restore a deleted entity (components + hierarchy links) or a removed component with its data intact. mini-ecs never reuses entity ids, which makes stale ids in the history degrade to safe no-ops. Timeline keyframe edits use a whole-animation snapshot command.

  `CommandHistory` fires an `on_scene_changed` hook on every execute/undo/redo — it marks the scene dirty (the title-bar `*`) and resets path-tracer accumulation in Render mode.

## Serialization

[`scene_manager.cpp`](../engine/src/scene/scene_manager.cpp) saves/loads scenes as JSON.

- Only entities **with a `TransformComponent`** are serialized; each component is written and read by its entry in the component registry (`me::ecs::components()`), so `scene_manager` just orchestrates the file I/O and the passes below.
- `save`/`load` come in two forms: a `(Registry&, path)` overload (decoupled from global state, used by the headless round-trip test) and a `(path)` convenience overload on the global registry.
- Scene files carry a `"version"` field; newer-format files load with a warning instead of being silently misread, and a malformed component is logged and skipped without abortingthe rest of the scene.
- Hierarchy is preserved with a three-pass load: pass 1 creates all entities and builds an `old_id → new_id` map; pass 2 loads each component through the registry; pass 3 re-links parents/children through the map (with warnings for dangling references).
- Play/Stop snapshots the scene by writing it to `.temp_play.json` and reloading it on stop — a simple way to restore edit-time state after simulation.
- Animations live in a sidecar next to the scene (`<scene>.anim.json`), saved with Ctrl+S and loaded whenever the scene loads (see the Animation section below).

## Assets & native handles

[`assets.cpp`](../engine/src/assets/assets.cpp) and [`audio.cpp`](../engine/src/audio/audio.cpp) are ref-counted caches keyed by a hash of the (virtual) path. Components store small `{handle}` ids (`TextureId`, `ModelId`, `SoundId`, `MusicId`) rather than raw raylib objects, so copying a component is cheap and the cache owns the GPU/audio resource. `0` means "none". Models also build a triangle BVH on load for the path tracer.

### Virtual file system

[`vfs.cpp`](../engine/src/core/vfs.cpp) maps schemes to physical folders. The editor mounts `root://` (next to the exe), `engine://` (its `assets/`), and `game://` (the open project's `assets/`). [`file_system.hpp`](../engine/include/mini-engine-raylib/core/file_system.hpp) wraps `std::filesystem` and auto-resolves `scheme://` paths.

## Rendering

- **Forward renderer** ([`renderer.cpp`](../engine/src/render/renderer.cpp)) loads `engine://shaders/lighting.{vs,fs}`, uploads up to 8 point lights + one directional light, binds per-object material uniforms, and draws `Shape3DComponent` primitives and `Model3DComponent` meshes using each transform's cached matrix.
- **Directional-light shadows**: `render::render_shadows(focus)` renders a depth-only pass from the sun's view into a depth texture (orthographic volume centered on `focus` — the viewer's camera — with texel snapping so edges don't shimmer); the lighting shader samples it with 3×3 PCF and a slope-scaled bias, attenuating only the directional light's contribution. The volume extent and map resolution are **per-light** fields on `DirectionalLightComponent` (inspector-tunable, serialized; defaults 60 units / 2048²) — the depth target is recreated lazily when the resolution changes. **Front-end contract**: the pass binds its own framebuffer, so call it once per frame *before* binding the final render target (the editor calls it before the viewport texture; the game runtime at the top of `on_render`). The depth pass reuses the same `draw_geometry` path as the main pass, so the two can't drift. Each `DirectionalLightComponent` carries a `cast_shadows` toggle; point lights don't cast real-time shadows (the path tracer shadows everything regardless).
- **Path tracer** ([`raytracer_system.cpp`](../engine/src/systems/raytracer_system.cpp)) has a CPU backend and a GPU compute-shader backend (`raytracer.comp`) kept feature-equal: next-event estimation, area-light soft shadows, inverse-square falloff, glass with Fresnel + Beer–Lambert tinting (per-material tint strength), depth of field with click-to-focus, exposure, firefly clamping, an art-directable sky, ACES tonemapping and progressive accumulation. The scene is flattened into SSBO-friendly arrays (nodes/triangles/materials/primitives/lights/emitters) and rebuilt on `reset_accumulation(&registry)`.
- **GPU dispatch is banded**: large frames render in horizontal row bands (budget scaled by bounce count) so no single dispatch can outlive the OS GPU watchdog (TDR) at export resolutions. On platforms without compute shaders (macOS caps OpenGL at 4.1) the GPU backend simply never initializes and the CPU backend keeps working.
- **Real-time path tracing (RT Play / raytraced games)**: `RaytracerSystem::render_realtime_frame` burst-renders `play_samples_per_frame` samples per game frame with a full scene rebuild (things moved), borrowing the progressive-accumulation machinery. With `lock_noise_pattern` the per-pixel seed salt resets every frame, freezing residual noise into a stable dither. The editor's toolbar "RT" toggle drives this in Play mode (viewport shows the raytracer output); exported games opt in via `"renderer": "raytraced"` + an `"rt"` settings block in `game_config.json` (written by the export dialog from the current Raytracer Settings) and upscale the low-res result with nearest filtering. Practical only at low internal resolutions — that's the retro/pixelated aesthetic the mode is for.
- **Feature toggles** (`enable_indirect` / `enable_soft_shadows` / `enable_reflections` / `enable_refraction`, plus `RenderQualityPreset` Full/Lite/Flat): every noise source in the integrator is a stochastic sample (diffuse GI direction, soft-shadow disk/cone, glossy jitter, DoF lens), so disabling the stochastic features makes the image fully deterministic — noise-free at one sample. The flags gate the *same* spots in both backends (CPU `trace_ray`, GPU `raytracer.comp`) and must stay in lockstep: soft-shadow-off aims shadow rays at light centers; refraction-off rewrites glass to a matte solid; the opaque scatter blends a specular end and a diffuse end whose presence the reflection/GI flags control (both off → no bounce). The flags ride along in the export `"rt"` block, so a shipped game renders with exactly the look set in the editor.
- **Offline rendering** is editor-side
  ([`editor_offline_render.cpp`](../editor/src/core/editor_offline_render.cpp)): the PNG export and the animation render own the raytracer while they run (the viewport pass and its resolution tracker pause), render at their own resolution/samples, and restore the viewport configuration afterwards.

## Animation

[`scene_animation.hpp`](../editor/include/editor/core/scene_animation.hpp) holds the data model; [`animation_panel.cpp`](../editor/src/panels/animation_panel.cpp) is the timeline UI.

- **Three track kinds**: a camera track (position, look-at, FOV, exposure, aperture, focus distance), an environment track (sky colors, sky intensity, ambient), and any number of entity tracks. Keyframes are snapshots captured at the playhead; frames between keys are interpolated (linear or smoothstep, per key).
- **Entity tracks reuse the component registry**: a key stores the component's scene-JSON snapshot, and evaluation lerps every numeric field (`lerp_json`) before loading the blend back onto the entity — any *data-only* component is animatable with zero per-component code (the panel whitelists Transform, Material, MeshRenderer, lights and Camera; asset- backed components would re-acquire handles every frame). Transform keeps its live hierarchy links across applies. Tracks reference entities by id in memory and by Tag name in the sidecar (ids change every scene load), re-resolving on load.
- **Scrub/preview applies the evaluated state to the live scene** — in Render mode that also rebuilds transforms and the BVH so the path tracer follows. The offline animation render walks output frames (`t = frame / fps`), letting each frame converge to the sample target before saving `frame_NNNN.png` (join with ffmpeg).
- Pure-math behavior (interpolation, easing, clamping, JSON-lerp semantics) is pinned by the headless [`animation_eval`](../tests/animation_eval.cpp) test.

## Extending the engine

### Adding a new component type

1. **Define** the struct in `engine/include/mini-engine-raylib/ecs/*.hpp`.
2. **Register** it once in [`component_registry.cpp`](../engine/src/ecs/component_registry.cpp): give it a scene-JSON `name`, a `save`/`load` pair, and — only if it owns a GPU/audio handle — an `on_destroy` (a handle-releasing `remove` is generated automatically). That single entry drives scene serialization, entity duplication, handle cleanup, **undoable add/remove**, and the animation system's snapshots.
3. **Inspect** it (editor): write a `draw_*` function in [`inspector_panel.cpp`](../editor/src/panels/inspector_panel.cpp) using the `ComponentSection` / `track_edit` helpers, and add one row to the `inspector_components()` table — that single row drives both the Inspector panel and the Add-Component menu.
4. **Animate** it (optional): if the component is pure data, add its registry name to the `kAnimatable` whitelist in [`animation_panel.cpp`](../editor/src/panels/animation_panel.cpp) — it becomes a timeline track with interpolation for free.
5. **Expose to Lua** (optional): bind it in [`script_manager.cpp`](../engine/src/scripting/script_manager.cpp).

Step 2 replaces what used to be four separate hand-maintained lists (save, load, duplicate, deletion); step 3's table did the same for the editor's panel dispatch and Add-Component menu. A component's whole footprint is now one engine registry entry + one editor table row (plus itsbespoke inspector body).

A headless [round-trip test](../tests/scene_roundtrip.cpp) (`scene_roundtrip_test`) exercises every registered component through save → load → save and guards the on-disk format.

### Adding an input action

Bind actions/axes by name (`me::input::bind_action`, `bind_axis`, `bind_digital_axis`); defaults live in [`input_defaults.cpp`](../engine/src/input/input_defaults.cpp). Query them with `action_down/pressed/released` and `axis_value`. Lua sees these through the `Input` table.

## Known gaps & future work

Every structural item from the original review is done — the registry-driven architecture, the unified per-frame order, build hygiene (CMake ≥ 3.21, explicit source lists, warning-clean builds), the transform cycle guard and the rest of the stability pass; the full history lives in [CHANGELOG.md](../CHANGELOG.md). What knowingly remains:

- **EventBus has no unsubscribe** — fine while subscribers are process-lifetime (they are), but `this`-capturing handlers would dangle if a subscriber were ever recreated.
- **Linux and macOS are untested in practice.** All dependencies are cross-platform and the presets exist, but no real build has been validated; on macOS the GPU path-tracer backend is permanently unavailable (no compute shaders in Apple's OpenGL — the CPU backend is the fallback, a Metal/wgpu backend the eventual answer).
- **Animation**: entity tracks re-bind by Tag *name* across scene loads (rename + save keeps them; duplicate names bind to the first match), and entity keys are edited by re-capture ("Update From Scene") rather than per-field editing.
- **Lua is the gameplay ceiling for shipped-engine users** — the binding surface in `script_manager.cpp` defines what exported games can do. The post-1.0 surface grew into a real gameplay API: entity lookup/spawning, scene switching, Vector3 math, component access, physics velocities/impulses, **collision callbacks** (Jolt ContactListener → buffered pair events → `on_collision_enter/exit` after the step), **trigger volumes** and **raycasts** — see [LUA_API.md](LUA_API.md). The biggest remaining physics gaps are capsule/mesh colliders and a character controller. Note: body sleeping is disabled — Jolt reports contact removal when an island sleeps, which would fire false `on_collision_exit` on resting bodies; at this engine's scene scale keeping bodies awake is the simple, correct trade.
