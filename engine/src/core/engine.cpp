#include "mini-engine-raylib/core/engine.hpp"

#include <memory>

#include <raylib.h>

#include "mini-engine-raylib/input/input.hpp" 
#include "mini-engine-raylib/input/input_defaults.hpp"
#include "mini-engine-raylib/audio/audio.hpp" 
#include "mini-engine-raylib/assets/assets.hpp"
#include "mini-engine-raylib/scripting/script_manager.hpp" 
#include "mini-engine-raylib/systems/script_system.hpp"
#include "mini-engine-raylib/systems/transform_system.hpp"

#include <mini-ecs/registry.hpp>

namespace me {

	struct EngineState {
		std::unique_ptr<Registry> registry;
		AppConfig config;
		bool running = false;
		bool is_playing = false;
	};

	static EngineState s_State;

	bool init(const AppConfig& config) {
		s_State.config = config;

		// 1. Raylib Window Initialization
		if (config.vsync) {
			SetConfigFlags(FLAG_VSYNC_HINT);
		}

		// Enable resizing
		SetConfigFlags(FLAG_WINDOW_RESIZABLE);

		InitWindow(config.width, config.height, config.title.c_str());
		SetExitKey(0); // Disable default ESC to close

		if (!config.vsync) {
			if (config.target_fps > 0) SetTargetFPS(config.target_fps);
			else SetTargetFPS(0);
		}

		// 2. Engine Subsystem Initialization
		me::input::setup_default_bindings();
		me::audio::init();
		me::audio::set_master_volume(0.9f);
		me::scripting::init(); // Lua brain

		// 3. ECS init
		s_State.registry = std::make_unique<Registry>();

		s_State.running = true;
		return true;
	}

	void run(Application& app, const AppConfig& config) {
		if (!init(config)) return;

		app.on_start();

		int last_width = s_State.config.width;
		int last_height = s_State.config.height;

		while (s_State.running && !WindowShouldClose()) {
			std::string title = s_State.config.title + " | FPS: " + std::to_string(GetFPS());
			SetWindowTitle(title.c_str());

			if (IsWindowResized()) {
				int current_width = GetScreenWidth();
				int current_height = GetScreenHeight();
				app.on_resize(current_width, current_height);
				last_width = current_width;
				last_height = current_height;
			}

			// -- Update Subsystems & Game --
			float dt = GetFrameTime();

			me::input::poll();

			// 1. Core Engine Systems
			if (s_State.is_playing) {
				me::systems::script_update(dt);
			}
			me::systems::transform_update();

			// 2. Application Logic (Editor or Game)
			app.on_update(dt);

			// 3. Rendering
			BeginDrawing();
			ClearBackground({ 0, 0, 0, 0 });
			app.on_render();
			EndDrawing();
		}

		// 4. User Game Shutdown
		app.on_shutdown();

		// 5. Engine Cleanup
		s_State.registry.reset();   // ECS Dies First (destroys Lua Script Components)
		me::scripting::shutdown();  // Lua Dies Second
		me::assets::release_all();  // Audio/Textures Die Last
		me::audio::shutdown();

		CloseWindow();
	}

	Registry& get_registry() {
		return *s_State.registry;
	}

	void set_playing(bool playing) {
		s_State.is_playing = playing;
	}

	bool is_playing() {
		return s_State.is_playing;
	}

	void close_application() {
		s_State.running = false;
	}

	int get_window_width() {
		return GetScreenWidth();
	}

	int get_window_height() {
		return GetScreenHeight();
	}

} // namespace me
