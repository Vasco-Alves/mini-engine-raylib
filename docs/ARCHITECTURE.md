# Architecture

This document describes how `mini-engine-raylib` is put together: the layering, the
per-frame update order, how scenes are serialized, and how to extend the engine. It
supersedes the older sketch in `system_order_updates.txt`, whose namespace/function names
(`me::scene::manager::Update`, `me::physics::Step`, `me::lifetime`, `me::animation`, …) no
longer match the code.

## Layers

```
        ┌─────────────────────────────────────────────┐
        │  editor  (ImGui tool, ImGuizmo, panels)      │   executable
        └───────────────────────┬─────────────────────┘
                                 │ depends on
        ┌───────────────────────▼─────────────────────┐
        │  engine  (me::*)                             │   static library
        │  core · ecs · systems · render · scene ·     │
        │  scripting · audio · assets · input          │
        └───────────────────────┬─────────────────────┘
                                 │ depends on
        ┌───────────────────────▼─────────────────────┐
        │  vendor  (raylib, jolt, lua/sol3, imgui,     │
        │  mini-ecs, nlohmann_json)                    │
        └─────────────────────────────────────────────┘
```

The key boundary: **`engine` has no dependency on ImGui or the editor.** The editor is one
front-end built on the engine's public API (`engine/include/mini-engine-raylib/...`). A
shipped game would subclass `me::Application` the same way the editor does.

## Application & engine loop

The engine exposes a small free-function API (`me::init`, `me::run`, `me::get_registry`,
play/pause/step controls) backed by a single static `EngineState`
([`engine.cpp`](../engine/src/core/engine.cpp)). A consumer subclasses
[`me::Application`](../engine/include/mini-engine-raylib/core/application.hpp) and overrides
`on_start` / `on_update` / `on_render` / `on_resize` / `on_shutdown`, then calls
`me::run(app, config)`.

`me::run` owns the main loop. Each frame it:

1. polls input and updates window state,
2. calls `me::world_update(dt)` — the canonical simulation step: **scripts → physics →
   transforms** (scripts and physics gated by play/pause/step; transforms always run),
3. calls `app.on_update(dt)`,
4. pumps streamed music (`me::audio::update()`),
5. brackets `app.on_render()` between `BeginDrawing` / `EndDrawing`,
6. processes deferred ECS deletions, releasing native handles (audio/model/texture) first.

`world_update` runs physics *before* the transform pass, so a body moved by physics gets its
world matrix rebuilt the same frame (no one-frame lag). The **spatial-audio** system and all
**rendering/tooling** stay consumer-side — the editor calls them in `on_update` / `on_render`,
since the audio listener and the viewport are front-end-specific.

### Per-frame order in the editor

```
me::run:
  input::poll
  world_update(dt):                            // shared simulation order
      (if playing) script_update -> physics::update   // logic sets velocities, Jolt integrates
      transform_update                                 // hierarchy → world matrices (post-physics)
  EditorApp::on_update(dt):
      file-drop import, editor camera fly/orbit
      (if rendering) raytracer.on_update(...)
      poll_shortcuts()
      systems::audio_update(...)               // 3D pan/attenuation, triggers (front-end listener)
  audio::update()                              // pump music streams
  EditorApp::on_render():
      viewport: render::render_world(editor cam) + gizmo/light/physics debug draw
      ImGui: panels, toolbar, menus, modals
  registry.process_deletions(...)
```

The canonical simulation order now lives in `me::world_update`; the older
`system_order_updates.txt` sketch predates it. Editor-only concerns (camera, tooling, spatial
audio) layer on top in `on_update` / `on_render`.

## ECS model

[mini-ecs](../vendor/mini-ecs) is a small archetype-less ECS:

- **Entities** are `uint32_t` ids (`me::entity::entity_id`); `0` is the reserved `null`
  value and ids start at `1`. `me::Entity` is a lightweight `{id, Registry*}` handle with a
  convenience component API.
- **Components** are plain structs (see `engine/include/mini-engine-raylib/ecs/`). No
  inheritance, no registration.
- **Pools** are dense arrays. Systems iterate via `registry.view<T>()` and access the
  parallel `entity_map[i]` / `components[i]` arrays directly.
- **Deletion is deferred**: `destroy_entity` marks an entity, and `process_deletions` (end
  of frame) runs a pre-delete callback so native GPU/audio handles can be released before
  the row disappears.

### Transforms & the scene graph

`TransformComponent` carries position/rotation(euler)/scale, a quaternion (the rotation
source of truth — the gizmo writes it directly to avoid Euler drift), a parent id + children
list, a cached `model_matrix`, and a dirty flag.
[`transform_system.cpp`](../engine/src/systems/transform_system.cpp) walks roots → children
recursively, rebuilding only dirty subtrees and propagating dirtiness downward. The
inspector edits Euler; the system syncs Euler→quat (or quat→Euler) depending on which
changed.

## Editor: selection, gizmos, and undo

- **Selection** is broadcast via the `EventBus` (`EntitySelectedEvent`); the hierarchy panel
  holds the current selection. `0xFFFFFFFF` is used as the "nothing selected" sentinel.
- **Gizmos** ([`viewport_panel.cpp`](../editor/src/panels/viewport_panel.cpp)) manipulate the
  world matrix via ImGuizmo, convert back to local space relative to the parent, and decompose
  into position/rotation/scale.
- **Undo/redo** ([`icommands.hpp`](../editor/include/editor/core/icommands.hpp)) uses a
  `ModifyComponentCommand<T>` that snapshots a component's whole value before and after an
  edit. The inspector captures `start_state` on `IsItemActivated` and pushes a command on
  `IsItemDeactivatedAfterEdit`; the gizmo does the same around a drag. `CommandHistory` also
  fires an `on_scene_changed` hook (used to reset path-tracer accumulation).

## Serialization

[`scene_manager.cpp`](../engine/src/scene/scene_manager.cpp) saves/loads scenes as JSON.

- Only entities **with a `TransformComponent`** are serialized; each component is written and
  read by its entry in the component registry (`me::ecs::components()`), so `scene_manager` just
  orchestrates the file I/O and the passes below.
- `save`/`load` come in two forms: a `(Registry&, path)` overload (decoupled from global state,
  used by the headless round-trip test) and a `(path)` convenience overload on the global registry.
- Hierarchy is preserved with a two-pass load: pass 1 creates all entities and builds an
  `old_id → new_id` map; pass 2 parses components and re-links parents/children through the
  map (with warnings for dangling references).
- Play/Stop snapshots the scene by writing it to `.temp_play.json` and reloading it on stop —
  a simple way to restore edit-time state after simulation.

## Assets & native handles

[`assets.cpp`](../engine/src/assets/assets.cpp) and [`audio.cpp`](../engine/src/audio/audio.cpp)
are ref-counted caches keyed by a hash of the (virtual) path. Components store small
`{handle}` ids (`TextureId`, `ModelId`, `SoundId`, `MusicId`) rather than raw raylib objects,
so copying a component is cheap and the cache owns the GPU/audio resource. `0` means "none".
Models also build a triangle BVH on load for the path tracer.

### Virtual file system

[`vfs.cpp`](../engine/src/core/vfs.cpp) maps schemes to physical folders. The editor mounts
`root://` (next to the exe), `engine://` (its `assets/`), and `game://` (the open project's
`assets/`). [`file_system.hpp`](../engine/include/mini-engine-raylib/core/file_system.hpp)
wraps `std::filesystem` and auto-resolves `scheme://` paths.

## Rendering

- **Forward renderer** ([`renderer.cpp`](../engine/src/render/renderer.cpp)) loads
  `engine://shaders/lighting.{vs,fs}`, uploads up to 8 point lights + one directional light,
  binds per-object material uniforms, and draws `Shape3DComponent` primitives and
  `Model3DComponent` meshes using each transform's cached matrix.
- **Path tracer** ([`raytracer_system.cpp`](../engine/src/systems/raytracer_system.cpp)) has a
  CPU backend and a GPU compute-shader backend (`raytracer.comp`), with progressive
  accumulation, an art-directable sky, and PNG export. It flattens the scene into SSBO-friendly
  arrays (nodes/triangles/materials/primitives/lights/emitters).

## Extending the engine

### Adding a new component type

1. **Define** the struct in `engine/include/mini-engine-raylib/ecs/*.hpp`.
2. **Register** it once in
   [`component_registry.cpp`](../engine/src/ecs/component_registry.cpp): give it a scene-JSON
   `name`, a `save`/`load` pair, and — only if it owns a GPU/audio handle — an `on_destroy`.
   That single entry drives scene serialization, entity duplication, and handle cleanup
   (`me::ecs::clone_entity` / `release_native_handles` iterate the registry).
3. **Inspect** it (editor): write a `draw_*` function in
   [`inspector_panel.cpp`](../editor/src/panels/inspector_panel.cpp) using the
   `ComponentSection` / `track_edit` helpers, and add one row to the `inspector_components()`
   table — that single row drives both the Inspector panel and the Add-Component menu.
4. **Expose to Lua** (optional): bind it in
   [`script_manager.cpp`](../engine/src/scripting/script_manager.cpp).

Step 2 replaces what used to be four separate hand-maintained lists (save, load, duplicate,
deletion); step 3's table did the same for the editor's panel dispatch and Add-Component menu.
A component's whole footprint is now one engine registry entry + one editor table row (plus its
bespoke inspector body).

A headless [round-trip test](../tests/scene_roundtrip.cpp) (`scene_roundtrip_test`) exercises
every registered component through save → load → save and guards the on-disk format.

### Adding an input action

Bind actions/axes by name (`me::input::bind_action`, `bind_axis`, `bind_digital_axis`);
defaults live in [`input_defaults.cpp`](../engine/src/input/input_defaults.cpp). Query them
with `action_down/pressed/released` and `axis_value`. Lua sees these through the `Input`
table.

## Recommended cleanups

The architecture is sound; most friction comes from hand-maintained per-component lists.
In rough priority:

1. **Centralize component registration.** *(Done.)*
   [`component_registry.cpp`](../engine/src/ecs/component_registry.cpp) drives scene save/load,
   entity duplication, and native-handle cleanup; the editor's `inspector_components()` table
   drives the Inspector panels and the Add-Component menu. New components register once per side.
2. **Finish or fence off the 2D path** (`render_2d`, sprites, `Shape2D`, `Camera2D`). Their data
   now serializes via the registry; what's missing is the editor actually drawing them.
3. **One sentinel for "no entity"** — replace literal `0xFFFFFFFF` with `me::entity::null`
   semantics consistently.

> Already addressed during the review: `me::audio::update()` is now pumped every frame
> (background music streams); the inspector no longer leaves the ImGui tree stack unbalanced
> when a component is removed while expanded; serialization/duplication/handle-cleanup are
> unified behind the component registry; `SpriteComponent` now persists (it was silently
> dropped on save before); and the per-frame simulation order is unified in `me::world_update`
> (scripts → physics → transforms), fixing a one-frame physics lag and removing the editor's
> separate physics-step path.
