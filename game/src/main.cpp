// The shipped game runtime: a minimal me::Application that loads a packaged
// project and runs it in play mode. Next to this executable the editor's
// "File > Export Game..." places:
//
//   <GameName>.exe       this runtime, renamed
//   game_config.json     window settings + which scene to boot
//   assets/shaders/      the engine's runtime shaders   (mounted as engine://)
//   data/                the project's exported content (mounted as game://)
//
// No editor, no ImGui — just the engine loop: scripts -> physics -> transforms,
// rendered from the scene's active CameraComponent.

#include <mini-engine-raylib/core/engine.hpp>
#include <mini-engine-raylib/core/application.hpp>
#include <mini-engine-raylib/core/vfs.hpp>
#include <mini-engine-raylib/core/events.hpp>
#include <mini-engine-raylib/core/logger.hpp>
#include <mini-engine-raylib/render/renderer.hpp>
#include <mini-engine-raylib/scene/scene_manager.hpp>
#include <mini-engine-raylib/systems/physics_system.hpp>
#include <mini-engine-raylib/systems/audio_system.hpp>
#include <mini-engine-raylib/systems/raytracer_system.hpp>
#include <mini-engine-raylib/ecs/components.hpp>
#include <mini-ecs/registry.hpp>

#include <raylib.h>
#include <raymath.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

namespace {

	// Capability marker the editor's exporter scans for inside this binary, so
	// it can refuse to package a feature the runtime was built without (e.g. a
	// raytraced export with a stale game.exe — the game would silently fall
	// back to raster). Logged at boot, which also keeps the string referenced
	// and alive through release-build optimization. One self-contained marker
	// per capability; never reword existing ones.
	constexpr const char* kCapRaytraced = "me-runtime-cap:raytraced;";

	struct GameConfig {
		std::string name = "Mini Engine Game";
		std::string main_scene = "game://scenes/main.json";
		int width = 1280;
		int height = 720;
		bool vsync = true;

		// "raytraced": render through the real-time path tracer instead of the
		// rasterizer (retro/pixelated games — the low internal resolution is what
		// buys the per-frame samples). Settings captured at export time.
		bool raytraced = false;
		float rt_resolution_scale = 0.25f;
		int rt_samples_per_frame = 8;
		int rt_bounces = 3;
		bool rt_lock_noise_pattern = true;
		float rt_exposure = 0.6f;
		float rt_firefly_clamp = 20.0f;
		float rt_ambient_strength = 0.03f;
		Vector3 rt_sky_horizon = { 1.0f, 1.0f, 1.0f };
		Vector3 rt_sky_zenith = { 0.5f, 0.7f, 1.0f };
		float rt_sky_intensity = 1.0f;
		bool rt_soft_shadows = true;
		bool rt_reflections = true;
		bool rt_refraction = true;
		bool rt_indirect = true;
	};

	Vector3 read_vec3(const nlohmann::json& j, const char* key, Vector3 fallback) {
		if (!j.contains(key) || !j[key].is_array() || j[key].size() < 3) return fallback;
		return { j[key][0].get<float>(), j[key][1].get<float>(), j[key][2].get<float>() };
	}

	GameConfig load_game_config(const std::filesystem::path& dir) {
		GameConfig cfg;
		std::ifstream ifs(dir / "game_config.json");
		if (!ifs) return cfg; // defaults still give a window + clear error logs
		try {
			nlohmann::json j;
			ifs >> j;
			cfg.name = j.value("name", cfg.name);
			cfg.main_scene = j.value("main_scene", cfg.main_scene);
			cfg.width = j.value("width", cfg.width);
			cfg.height = j.value("height", cfg.height);
			cfg.vsync = j.value("vsync", cfg.vsync);
			cfg.raytraced = (j.value("renderer", "raster") == "raytraced");
			if (cfg.raytraced && j.contains("rt")) {
				const auto& rt = j["rt"];
				cfg.rt_resolution_scale = rt.value("resolution_scale", cfg.rt_resolution_scale);
				cfg.rt_samples_per_frame = rt.value("samples_per_frame", cfg.rt_samples_per_frame);
				cfg.rt_bounces = rt.value("bounces", cfg.rt_bounces);
				cfg.rt_lock_noise_pattern = rt.value("lock_noise_pattern", cfg.rt_lock_noise_pattern);
				cfg.rt_exposure = rt.value("exposure", cfg.rt_exposure);
				cfg.rt_firefly_clamp = rt.value("firefly_clamp", cfg.rt_firefly_clamp);
				cfg.rt_ambient_strength = rt.value("ambient_strength", cfg.rt_ambient_strength);
				cfg.rt_sky_horizon = read_vec3(rt, "sky_horizon", cfg.rt_sky_horizon);
				cfg.rt_sky_zenith = read_vec3(rt, "sky_zenith", cfg.rt_sky_zenith);
				cfg.rt_sky_intensity = rt.value("sky_intensity", cfg.rt_sky_intensity);
				cfg.rt_soft_shadows = rt.value("soft_shadows", cfg.rt_soft_shadows);
				cfg.rt_reflections = rt.value("reflections", cfg.rt_reflections);
				cfg.rt_refraction = rt.value("refraction", cfg.rt_refraction);
				cfg.rt_indirect = rt.value("indirect", cfg.rt_indirect);
			}
		} catch (...) {
			me::logger::warn("game_config.json could not be parsed - using defaults.");
		}
		return cfg;
	}

	class GameApp : public me::Application {
	public:
		explicit GameApp(GameConfig cfg) : m_Config(std::move(cfg)) {}

		void on_start() override {
			// The engine logs through the event bus (the editor shows them in its
			// console panel); a shipped game has no console panel, so echo them to
			// stdout — otherwise load errors in an exported game vanish silently.
			me::get_event_bus().subscribe<me::events::LogEvent>([](me::events::LogEvent* e) {
				const char* tag = e->level == me::logger::LogLevel::Error ? "[ERROR] "
					: e->level == me::logger::LogLevel::Warning ? "[WARN]  " : "[INFO]  ";
				std::printf("%s%s\n", tag, e->message.c_str());
				std::fflush(stdout);
				});

			me::logger::info(std::string("Runtime capabilities: ") + kCapRaytraced);

			// Mounts mirror the editor's: engine shaders next to the exe, the
			// packaged project content under data/.
			me::vfs::mount("engine", "assets");
			me::vfs::mount("game", "data");

			// Lua's Engine.quit(): in the shipped game it closes the window.
			// (The flag is only read after the frame, so firing mid-script is safe.)
			me::get_event_bus().subscribe<me::events::QuitRequestedEvent>([](me::events::QuitRequestedEvent*) {
				me::close_application();
				});

			me::render::init();
			me::physics::init();

			if (!me::scene_manager::load(m_Config.main_scene)) {
				me::logger::error("Could not load the main scene: " + m_Config.main_scene);
			}

			// Raytraced renderer: the real-time path tracer at a low internal
			// resolution, upscaled to the window (nearest filter = crisp pixels).
			if (m_Config.raytraced) {
				m_Raytracer = std::make_unique<me::systems::RaytracerSystem>();
				m_Raytracer->play_samples_per_frame = m_Config.rt_samples_per_frame;
				m_Raytracer->lock_noise_pattern = m_Config.rt_lock_noise_pattern;
				m_Raytracer->max_bounces = m_Config.rt_bounces;
				m_Raytracer->resolution_scale = m_Config.rt_resolution_scale;
				m_Raytracer->exposure = m_Config.rt_exposure;
				m_Raytracer->firefly_clamp = m_Config.rt_firefly_clamp;
				m_Raytracer->ambient_strength = m_Config.rt_ambient_strength;
				m_Raytracer->sky_horizon_color = m_Config.rt_sky_horizon;
				m_Raytracer->sky_zenith_color = m_Config.rt_sky_zenith;
				m_Raytracer->sky_intensity = m_Config.rt_sky_intensity;
				m_Raytracer->enable_soft_shadows = m_Config.rt_soft_shadows;
				m_Raytracer->enable_reflections = m_Config.rt_reflections;
				m_Raytracer->enable_refraction = m_Config.rt_refraction;
				m_Raytracer->enable_indirect = m_Config.rt_indirect;
				m_Raytracer->on_start(internal_w(me::get_window_width()), internal_h(me::get_window_height()));
				me::logger::info("Renderer: raytraced (" + std::to_string(m_Raytracer->get_width()) + "x"
					+ std::to_string(m_Raytracer->get_height()) + " internal, "
					+ std::to_string(m_Config.rt_samples_per_frame) + " spp/frame)");
			}

			// The runtime IS play mode: start simulating immediately.
			me::physics::on_play(me::get_registry());
			me::set_playing(true);
			me::get_event_bus().publish<me::events::PlayStateChangedEvent>(true);
		}

		void on_update(float dt) override {
			(void)dt; // scripts/physics advance in the engine's world_update

			// 3D audio follows the active camera.
			const me::components::TransformComponent* t = nullptr;
			const me::components::CameraComponent* c = nullptr;
			find_view(t, c);

			Vector3 pos = { t->position.x, t->position.y, t->position.z };
			Vector3 target = { c->target.x, c->target.y, c->target.z };
			Vector3 fwd = Vector3Normalize(Vector3Subtract(target, pos));
			Vector3 up = Vector3Normalize({ c->up.x, c->up.y, c->up.z });
			me::systems::audio_update(me::get_registry(), pos, fwd, up);

			// The path-traced frame is rendered here, outside the draw bracket
			// (same flow the editor uses); on_render just blits the result.
			if (m_Raytracer) m_Raytracer->render_realtime_frame(me::get_registry(), *c, *t);
		}

		void on_render() override {
			if (m_Raytracer) {
				ClearBackground(BLACK);
				Texture2D* tex = m_Raytracer->get_texture();
				Rectangle src = { 0, 0, (float)tex->width, (float)tex->height };
				Rectangle dst = { 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight() };
				// raylib textures default to point filtering — chunky pixels for free.
				DrawTexturePro(*tex, src, dst, { 0, 0 }, 0.0f, WHITE);
				return;
			}

			const me::components::TransformComponent* t = nullptr;
			const me::components::CameraComponent* c = nullptr;
			find_view(t, c);

			// Shadow pass first (it binds its own framebuffer), then the world.
			me::render::render_shadows({ t->position.x, t->position.y, t->position.z });
			me::render::clear_world(me::Color{ 30, 30, 30, 255 });
			me::render::render_world(t, c);
		}

		void on_resize(int width, int height) override {
			if (m_Raytracer) m_Raytracer->resize(internal_w(width), internal_h(height));
		}

		void on_shutdown() override {
			if (m_Raytracer) m_Raytracer->on_stop();
			me::physics::on_stop();
		}

	private:
		int internal_w(int window_w) const { return std::max(1, (int)(window_w * m_Config.rt_resolution_scale)); }
		int internal_h(int window_h) const { return std::max(1, (int)(window_h * m_Config.rt_resolution_scale)); }

		// The scene's active camera — or a fixed fallback view, so a scene
		// without a camera shows the world instead of a black screen.
		void find_view(const me::components::TransformComponent*& out_t,
			const me::components::CameraComponent*& out_c) {
			static me::components::TransformComponent s_fallback_t{
				{ 0.0f, 5.0f, 10.0f }, { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f } };
			static me::components::CameraComponent s_fallback_c{};
			out_t = &s_fallback_t;
			out_c = &s_fallback_c;

			auto& reg = me::get_registry();
			for (auto [e, cam] : reg.view<me::components::CameraComponent>()) {
				if (!cam.active) continue;
				if (auto* t = reg.try_get_component<me::components::TransformComponent>(e)) {
					out_t = t;
					out_c = &cam;
					return;
				}
			}
		}

		GameConfig m_Config;
		std::unique_ptr<me::systems::RaytracerSystem> m_Raytracer; // null = raster renderer
	};

} // namespace

int main() {
	// Run relative to the executable so double-clicking from anywhere works.
	std::filesystem::path app_dir = GetApplicationDirectory();
	std::error_code ec;
	std::filesystem::current_path(app_dir, ec);

	GameConfig cfg = load_game_config(app_dir);

	me::AppConfig engine_cfg;
	engine_cfg.title = cfg.name;
	engine_cfg.width = cfg.width;
	engine_cfg.height = cfg.height;
	engine_cfg.vsync = cfg.vsync;

	GameApp app(cfg);
	me::run(app, engine_cfg);
	return 0;
}
