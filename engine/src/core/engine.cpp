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

#include "mini-engine-raylib/ecs/components.hpp"
#include "mini-engine-raylib/ecs/audio_components.hpp"

namespace me {

	struct EngineState {
		std::unique_ptr<Registry> registry;
		AppConfig config;
		bool running = false;
		bool is_playing = false;
		bool is_paused = false;
		int step_frames = 0;
	};

	static EngineState s_State;

	bool init(const AppConfig& config) {
		s_State.config = config;

		if (config.vsync)
			SetConfigFlags(FLAG_VSYNC_HINT);

		SetConfigFlags(FLAG_WINDOW_RESIZABLE);

		InitWindow(config.width, config.height, config.title.c_str());
		SetExitKey(0);

		if (!config.vsync) {
			if (config.target_fps > 0) SetTargetFPS(config.target_fps);
			else SetTargetFPS(0);
		}

		me::input::setup_default_bindings();
		me::audio::init();
		me::audio::set_master_volume(0.9f);
		me::scripting::init();

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

			if (s_State.is_playing) {
				if (!s_State.is_paused) {
					me::systems::script_update(dt);
				} else if (s_State.step_frames > 0) {
					float fixed_dt = 1.0f / 60.0f;
					me::systems::script_update(fixed_dt);
					s_State.step_frames--;
				}
			}

			me::systems::transform_update();

			app.on_update(dt);

			BeginDrawing();
			ClearBackground({ 0, 0, 0, 0 });
			app.on_render();
			EndDrawing();

			// ==========================================
			// 4. PROCESS ECS DEFERRED DELETIONS
			// ==========================================
			s_State.registry->process_deletions([&](me::entity::entity_id e) {
				// Safely release native hardware handles before the entity is destroyed
				if (auto* audio = s_State.registry->try_get_component<me::components::AudioSourceComponent>(e)) {
					if (audio->clip.handle != 0) me::audio::release(audio->clip);
				}
				if (auto* bgm = s_State.registry->try_get_component<me::components::BackgroundMusicComponent>(e)) {
					if (bgm->stream.handle != 0) me::audio::release(bgm->stream);
				}
				if (auto* model = s_State.registry->try_get_component<me::components::Model3DComponent>(e)) {
					if (model->model.handle != 0) me::assets::release(model->model);
				}
				});
		}

		app.on_shutdown();

		s_State.registry.reset();
		me::scripting::shutdown();
		me::assets::release_all();
		me::audio::shutdown();

		CloseWindow();
	}

	Registry& get_registry() { return *s_State.registry; }
	void set_playing(bool playing) { s_State.is_playing = playing; }
	bool is_playing() { return s_State.is_playing; }
	void close_application() { s_State.running = false; }
	int get_window_width() { return GetScreenWidth(); }
	int get_window_height() { return GetScreenHeight(); }
	void set_paused(bool paused) { s_State.is_paused = paused; }
	bool is_paused() { return s_State.is_paused; }
	void step(int frames) { if (s_State.is_paused) s_State.step_frames = frames; }

} // namespace me