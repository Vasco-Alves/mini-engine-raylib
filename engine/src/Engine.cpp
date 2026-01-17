#include "Engine.hpp"
#include "Input.hpp"
#include "InputDefaults.hpp"
#include "Audio.hpp"
#include "Assets.hpp"
#include "Entity.hpp"

#include <raylib.h>

namespace me {
	namespace {
		bool s_Initialized = false;
		EngineConfig s_Cfg{};
	}

	bool Init(const EngineConfig& cfg) {
		if (s_Initialized) return true;

		// Configure vsync before window creation (raylib reads the hint at InitWindow)
		if (cfg.vsync) {
			SetConfigFlags(FLAG_VSYNC_HINT);
		}

		InitWindow(cfg.width, cfg.height, cfg.title);
		SetExitKey(0); // disable default Esc-to-exit (we handle this via input actions)

		if (!cfg.vsync) {
			if (cfg.targetFps > 0) SetTargetFPS(cfg.targetFps);
			else SetTargetFPS(0); // uncapped (raylib treats 0 as uncapped)
		}

		s_Cfg = cfg;
		s_Initialized = true;

		me::input::SetupDefaultBindings();
		me::audio::Init(); // optional (lazy-inits if you forget)
		me::audio::SetMasterVolume(0.9f);

		return true;
	}

	bool Update() {
		if (!s_Initialized) return false;
		if (WindowShouldClose()) return false;

		me::input::Poll();

		return true;
	}

	// TODO: input polling, timers, fixed timestep accumulation (later)

	void BeginFrame() {
		// NOTE: Avoid BeginDrawing/EndDrawing nesting across threads/scopes.
		BeginDrawing();
		ClearBackground(::RAYWHITE); // Later: use a render pipeline & camera clear color
		// TODO: render prep / submit draw lists
	}

	void EndFrame() {
		// TODO: debug overlays or UI can be drawn right before EndDrawing
		EndDrawing();
	}

	void Shutdown() {
		if (!s_Initialized) return;

		// Clean up engine-owned resources before closing the window
		me::DestroyAllEntities();   // clears registry & releases sprite textures via components
		me::assets::ReleaseAll();  // just in case anything remains loaded
		me::audio::Shutdown();     // stop & unload all sounds/music, close audio device

		CloseWindow();
		s_Initialized = false;
	}
}
