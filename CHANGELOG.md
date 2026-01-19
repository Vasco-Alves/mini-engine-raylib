# Changelog

All notable changes to this project will be documented in this file.

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
