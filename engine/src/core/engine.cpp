#include "mini-engine-raylib/core/engine.hpp"

// Assuming these exist and we will eventually update them to snake_case too
#include "input/Input.hpp" 
#include "mini-engine-raylib/input/input_defaults.hpp"
#include "audio/Audio.hpp"
#include "assets/Assets.hpp"

#include <mini-ecs/registry.hpp>
#include <raylib.h>
#include <memory>

namespace me {

	struct EngineState {
		std::unique_ptr<Registry> registry;
		AppConfig config;
		bool running = false;
	};

	static EngineState s_State;

	bool init(const AppConfig& config) {
		s_State.config = config;

		// 1. Raylib Window Initialization
		if (config.vsync) {
			SetConfigFlags(FLAG_VSYNC_HINT);
		}

		InitWindow(config.width, config.height, config.title.c_str());
		SetExitKey(0); // Disable default ESC to close

		if (!config.vsync) {
			if (config.target_fps > 0) SetTargetFPS(config.target_fps);
			else SetTargetFPS(0);
		}

		// 2. Engine Subsystem Initialization (UPDATED TO SNAKE_CASE)
		me::input::setup_default_bindings();
		me::audio::init();
		me::audio::set_master_volume(0.9f);

		// 3. ECS Init
		s_State.registry = std::make_unique<Registry>();

		s_State.running = true;
		return true;
	}

	void run(Application& app, const AppConfig& config) {
		if (!init(config)) return;

		app.on_start(config.width, config.height);

		int last_width = s_State.config.width;
		int last_height = s_State.config.height;

		double accumulator = 0.0;
		int frames = 0;

		while (s_State.running && !WindowShouldClose()) {
			float dt = GetFrameTime();

			accumulator += dt;
			frames++;
			if (accumulator >= 1.0) {
				std::string title = s_State.config.title + " | FPS: " + std::to_string(frames);
				SetWindowTitle(title.c_str());
				frames = 0;
				accumulator = 0.0;
			}

			if (IsWindowResized()) {
				int current_width = GetScreenWidth();
				int current_height = GetScreenHeight();
				app.on_resize(current_width, current_height);
				last_width = current_width;
				last_height = current_height;
			}

			// -- Update Subsystems & Game (UPDATED TO SNAKE_CASE) --
			me::input::poll();
			app.on_update(dt);

			BeginDrawing();
			ClearBackground({ 25, 25, 30, 255 });
			app.on_render();
			EndDrawing();
		}

		app.on_shutdown();

		// 5. Engine Cleanup
		s_State.registry.reset();
		me::assets::release_all();
		me::audio::shutdown();

		CloseWindow();
	}

	Registry& get_registry() {
		return *s_State.registry;
	}

	void close_application() {
		s_State.running = false;
	}

} // namespace me