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
#include <mini-engine-raylib/ecs/components.hpp>
#include <mini-ecs/registry.hpp>

#include <raylib.h>
#include <raymath.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <string>

namespace {

	struct GameConfig {
		std::string name = "Mini Engine Game";
		std::string main_scene = "game://scenes/main.json";
		int width = 1280;
		int height = 720;
		bool vsync = true;
	};

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
		} catch (...) {
			me::logger::warn("game_config.json could not be parsed - using defaults.");
		}
		return cfg;
	}

	class GameApp : public me::Application {
	public:
		explicit GameApp(GameConfig cfg) : m_Config(std::move(cfg)) {}

		void on_start() override {
			// Mounts mirror the editor's: engine shaders next to the exe, the
			// packaged project content under data/.
			me::vfs::mount("engine", "assets");
			me::vfs::mount("game", "data");

			me::render::init();
			me::physics::init();

			if (!me::scene_manager::load(m_Config.main_scene)) {
				me::logger::error("Could not load the main scene: " + m_Config.main_scene);
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
		}

		void on_render() override {
			const me::components::TransformComponent* t = nullptr;
			const me::components::CameraComponent* c = nullptr;
			find_view(t, c);

			me::render::clear_world(me::Color{ 30, 30, 30, 255 });
			me::render::render_world(t, c);
		}

		void on_shutdown() override {
			me::physics::on_stop();
		}

	private:
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
			auto& cams = reg.view<me::components::CameraComponent>();
			for (size_t i = 0; i < cams.size(); ++i) {
				if (!cams.components[i].active) continue;
				if (auto* t = reg.try_get_component<me::components::TransformComponent>(cams.entity_map[i])) {
					out_t = t;
					out_c = &cams.components[i];
					return;
				}
			}
		}

		GameConfig m_Config;
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
