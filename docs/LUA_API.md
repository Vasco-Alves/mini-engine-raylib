# Lua scripting reference

Attach a `.lua` file to an entity (drag it onto the Inspector's script drop-zone). Scripts run while the scene is **playing** — in the editor's play mode and in exported games — and hot-reload when the file is saved.

A script defines any of these optional functions:

```lua
function start(self)        -- called once, the first frame the script runs
end

function update(self, dt)   -- called every frame; dt is the frame time in seconds
end

function on_collision_enter(self, other)  -- this entity began touching `other`
end

function on_collision_exit(self, other)   -- this entity stopped touching `other`
end
```

`self` is the [Entity](#entity) the script is attached to. Each script instance runs in its own environment, so two scripts (or two entities sharing one script file) don't see each other's globals.

The collision callbacks fire right after the physics step, once per entity **pair** (a body resting on the floor is one enter, not one per contact point). Both entities need a RigidBody + collider; `other` may already be destroyed when the exit arrives (it answers `is_valid() == false`). Mark a RigidBody **Is Trigger** in the inspector to make it a sensor: overlaps fire these same callbacks but produce no physical response — pickups, damage zones, level exits.

The standard libraries `base`, `math`, `string` and `table` are open; `io` and `os` are deliberately not (scripts stay sandboxed). `print(...)` writes to the editor console / engine log.

---

## Entity

The handle to a scene entity. All component getters return `nil` when the component is missing, and every method is safe to call on a destroyed entity (it just does nothing).

| Method | Description |
| --- | --- |
| `e:is_valid()` | `false` once the entity has been destroyed. |
| `e:destroy()` | Destroys the entity at the end of the frame (its Jolt body is removed with it). |
| `e:get_id()` | The numeric entity id (ids are never reused). |
| `e:get_name()` / `e:set_name(name)` | The entity's Tag name (what `Scene.find` matches). |
| `e:get_transform()` | [TransformComponent](#components) or `nil`. |
| `e:get_material()` | [MaterialComponent](#components) or `nil`. |
| `e:get_light()` / `e:get_directional_light()` | Light components or `nil`. |
| `e:get_camera()` | [CameraComponent](#components) or `nil`. |
| `e:set_look(yaw, pitch)` | First-person camera aim: points the entity's Camera from yaw/pitch (degrees), from the entity's **world** position (so a Camera parented to a player at eye height works). Pitch is clamped to ±89.9°. Needs both a Transform and a Camera. |
| `e:add_offset(x, y, z)` | Moves the transform by a delta (non-physics movement). |
| `e:teleport(x, y, z)` | Moves the transform **and** the Jolt body, so a physics entity doesn't snap back. Velocity is kept — call `set_velocity(0,0,0)` too for a clean respawn. |

### Physics (requires a RigidBody + collider; no-ops otherwise)

| Method | Description |
| --- | --- |
| `e:set_velocity(x, y, z)` | Sets linear velocity in world space. |
| `e:set_local_velocity(x, y, z)` | Sets linear velocity along the entity's local axes. |
| `e:get_velocity()` | Current linear velocity as a `Vector3` (`(0,0,0)` without a live body). |
| `e:apply_impulse(x, y, z)` | Instantaneous impulse at the center of mass (mass × Δvelocity). |
| `e:set_angular_velocity(x, y, z)` | Angular velocity in radians/second, world space. |

Physics entities should be **root** entities (no parent): body transforms are written back to the entity's local transform.

### Audio

| Method | Description |
| --- | --- |
| `e:play_sound()` | Plays the entity's AudioSource clip (3D-spatialized if enabled). |
| `e:play_music()` / `e:stop_music()` / `e:pause_music()` / `e:resume_music()` | Controls the entity's BackgroundMusic stream. |

---

## Scene

| Function | Description |
| --- | --- |
| `Scene.find(name)` | First alive entity whose Tag matches, or `nil`. |
| `Scene.find_all(name)` | Array-style table of every match. |
| `Scene.create(name)` | A fresh empty entity (Tag + Transform), like the editor's Create Entity. |
| `Scene.spawn(template)` | Clones an existing entity and returns the clone. |
| `Scene.load(path)` | Switches to another scene at the **end of the frame**. |

**`Scene.spawn` is the poor man's prefab**: keep a template entity in the scene (park it below the floor or far away) and spawn copies of it. The clone gets its own copies of every component — fresh script instances (their `start` runs), its own asset references, and its own physics body, live immediately if the simulation is running. It joins the template's parent but never adopts its children.

```lua
local template = Scene.find("BulletTemplate")
local bullet = Scene.spawn(template)
bullet:teleport(muzzle.x, muzzle.y, muzzle.z)
bullet:set_velocity(dir.x * 40, dir.y * 40, dir.z * 40)
```

**`Scene.load` is how levels and menus work.** The path is a VFS path — in a project (and in an exported game) scenes live under `game://scenes/`:

```lua
if Input.action_pressed("Jump") then
    Scene.load("game://scenes/level_2.json")
end
```

The switch is deferred to the end of the frame, so the rest of the current frame finishes on the old scene. The physics world is rebuilt around the new scene and its scripts `start` on the next frame. In the editor, stopping play still returns you to the scene you were editing.

---

## Physics

| Function | Description |
| --- | --- |
| `Physics.raycast(origin, direction, max_distance)` | Closest physics body along the ray, or `nil`. `direction` need not be normalized; `max_distance` defaults to 1000. |

A hit is a table with `entity`, `point` (Vector3), `normal` (Vector3) and `distance`. Only physics bodies are seen (RigidBody + collider — render-only meshes are invisible to rays), and only while the simulation runs.

```lua
-- ground probe: how far is the floor below us?
local t = self:get_transform()
local hit = Physics.raycast(t.position, Vector3(0, -1, 0), 2.0)
if hit and hit.distance < 1.1 then
    -- close enough to jump
end
```

(For a grounded check, the collision callbacks with a contact counter — see the demo's `jump.lua` — are usually the better tool; raycasts shine for line-of-sight, shooting and probing ahead of movement.)

---

## Vector3

Construct with `Vector3(x, y, z)` (or `Vector3.new(x, y, z)`); fields `x`, `y`, `z` are read/write.

Operators: `+`, `-` (binary and unary), `*` (vector × number, both orders), `/` (vector ÷ number), `==`, `tostring`.

| Method | Description |
| --- | --- |
| `v:length()` / `v:length_sq()` | Magnitude / squared magnitude. |
| `v:normalized()` | Unit-length copy (the original is untouched). |
| `v:dot(other)` / `v:cross(other)` | Dot / cross product. |
| `v:distance(other)` | Distance between two points. |
| `v:lerp(other, t)` | Linear interpolation, `t` in [0, 1]. |

```lua
local to_player = (player_pos - self_pos):normalized()
e:set_velocity(to_player.x * speed, to_player.y * speed, to_player.z * speed)
```

## Color

`Color(r, g, b)` or `Color(r, g, b, a)`, channels 0–255; fields `r`, `g`, `b`, `a` are read/write.

---

## Components

Component objects returned by the entity getters are **live references** — assigning a field changes the scene immediately.

| Component | Fields |
| --- | --- |
| `TransformComponent` | `position`, `rotation` (Euler degrees), `scale` — all `Vector3`. |
| `MaterialComponent` | `albedo` (Color), `roughness`, `metallic`, `emission_power`, `transmission`, `ior`, `tint_strength`. |
| `LightComponent` | `color`, `intensity`, `radius`. |
| `DirectionalLightComponent` | `color`, `intensity`, `angular_radius`. |
| `CameraComponent` | `target` (Vector3), `up` (Vector3), `fov`, `active`. |

A camera that follows its entity's target is one line per frame:

```lua
function update(self, dt)
    local player = Scene.find("Player")
    if player then
        self:get_camera().target = player:get_transform().position
    end
end
```

---

## Input

Actions and axes are bound by name in C++ (`engine/src/input/input_defaults.cpp`); defaults include `MoveX`/`MoveZ` (WASD), `LookX`/`LookY` (mouse), `Jump` (Space) and the mouse buttons.

| Function | Description |
| --- | --- |
| `Input.action_down(name)` | Held this frame. |
| `Input.action_pressed(name)` | Went down this frame. |
| `Input.action_released(name)` | Went up this frame. |
| `Input.axis_value(name)` | Axis value (e.g. `-1..1` for `MoveX`, mouse delta for `LookX`/`LookY`). |
| `Input.lock_cursor()` / `Input.unlock_cursor()` | Capture/release the mouse. Lock it for mouse-look (the cursor hides and recenters so `LookX`/`LookY` keep flowing); unlock for menus. The editor force-unlocks when you Stop, so a locked cursor is never stranded. |

### Playtesting in the editor (focus)

When you press Play in the editor the game starts **unfocused**: it receives no input and the cursor stays free, so you can use the panels (tweak Raytracer Settings, inspect entities) without the camera reacting. **Click the viewport** to focus — input flows to the game and its cursor choice takes effect (an FPS that called `Input.lock_cursor()` locks now). **Press Escape** to unfocus again: the editor takes the mouse back and freezes the game's input, no matter what the game asked for.

So there are two independent things controlling the cursor, which is the key distinction:

- **The game's intent** — `Input.lock_cursor()` / `unlock_cursor()`. This is what a shipped game uses (lock for first-person look, unlock for a menu). It applies whenever the game is focused.
- **The editor's playtest focus** — click to focus, Escape to release. While unfocused the editor *overrides* the game's intent and frees the cursor, so you can always get back to the tools. A shipped game has no editor, so it's always "focused" and only the game's intent matters.

This means yes — you can test a game that unlocks its own cursor for UI, and still Escape out to change settings: the editor's release wins while you're unfocused, and the game's choice resumes when you click back in. (Editor-only shortcuts like the gizmo keys, Delete and save are also suppressed during Play, so they never fire on your game's keys.)

### A first-person controller

The recommended rig is two entities:

- **Player** (root) — a `RigidBody` (Dynamic) + collider, with **Freeze Rotation X and Z** ticked in the inspector so it can't tip over while walking, and the script below.
- **PlayerCamera** (a *child* of Player, dragged under it in the hierarchy) — a `Transform` raised to eye height (e.g. local Y ≈ 0.7) and an active `Camera`. As a child it follows the player automatically, and `set_look` aims it from its world position.

The script lives on the player, finds the camera child once, and drives both look and movement:

```lua
local yaw, pitch = 0, 0
local cam
local speed = 6

function start(self)
    Input.lock_cursor()
    cam = Scene.find("PlayerCamera")        -- the child camera at eye height
end

function update(self, dt)
    -- mouse look: aim the child camera (pitch auto-clamped to ±89.9°)
    yaw   = yaw   - Input.axis_value("LookX")
    pitch = pitch - Input.axis_value("LookY")
    if cam then cam:set_look(yaw, pitch) end

    -- walk in the facing direction. The body's rotation is frozen, so we build
    -- the move vector from yaw ourselves (forward at yaw 0 is +Z).
    local fx, fz = math.sin(math.rad(yaw)),  math.cos(math.rad(yaw))   -- forward
    local rx, rz = math.cos(math.rad(yaw)), -math.sin(math.rad(yaw))   -- right
    local f = -Input.axis_value("MoveZ")
    local s = -Input.axis_value("MoveX")
    local vy = self:get_velocity().y                                   -- keep gravity
    self:set_velocity((fx*f + rx*s) * speed, vy, (fz*f + rz*s) * speed)

    if Input.action_pressed("Jump") then self:apply_impulse(0, 5, 0) end
end
```

(You *can* put the Camera directly on the player instead of using a child — `set_look` works either way — but then the eye sits at the body's center rather than head height. The child gives you a proper eyeline.)

## Engine

| Function | Description |
| --- | --- |
| `Engine.quit()` | Ends the game session: an exported game closes; the editor leaves play mode. |
| `Engine.time()` | Seconds since the application started. |
| `Engine.warn(msg)` / `Engine.error(msg)` | Log at warning/error level (plain `print` logs at info). |

---

## What's not here yet

- Adding/removing components from Lua — author the components in the editor; `Scene.spawn` covers runtime creation.
- Asset loading from Lua — assets come in through the components placed in the editor.
- Capsule/mesh colliders and a true character controller — box and sphere shapes only for now (freeze rotation X+Z to keep a box/sphere player upright in the meantime).
