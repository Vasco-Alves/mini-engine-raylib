#include "mini-engine-raylib/scene/scene.hpp"
#include "mini-engine-raylib/core/engine.hpp"
#include "mini-engine-raylib/ecs/components.hpp"

#include <mini-ecs/registry.hpp>

#include <nlohmann/json.hpp>
#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <iostream>

using json = nlohmann::ordered_json;
namespace fs = std::filesystem;

namespace me {

	// ===================================================================
	// SCENE MANAGER IMPLEMENTATION
	// ===================================================================
	namespace scene_manager {

		namespace {
			std::unordered_map<std::string, Scene*> s_scenes;
			std::string s_current_name;
		}

		void register_scene(Scene* scene) {
			if (!scene) return;
			std::string name = scene->get_name();
			s_scenes[name] = scene;

			// If it has a file, ensure it exists
			const char* file = scene->get_file();
			if (file && *file) {
				fs::path scene_folder = fs::current_path() / "scenes";
				fs::create_directories(scene_folder);
				fs::path full_path = scene_folder / file;
				if (!fs::exists(full_path)) {
					std::ofstream out(full_path, std::ios::binary);
					if (out) out << R"({ "entities": [] })";
				}
			}

			scene->on_register();
		}

		void load(const std::string& name) {
			if (!s_current_name.empty() && s_scenes[s_current_name]) {
				s_scenes[s_current_name]->on_exit();
			}

			if (s_scenes.find(name) == s_scenes.end()) {
				std::cerr << "Scene not registered: " << name << "\n";
				return;
			}

			Scene* scene = s_scenes[name];
			scene->load_from_file(); // Load JSON if it has one

			s_current_name = name;
			scene->on_enter();
		}

		void update(float dt) {
			if (!s_current_name.empty() && s_scenes[s_current_name])
				s_scenes[s_current_name]->on_update(dt);
		}

		void exit() {
			if (!s_current_name.empty() && s_scenes[s_current_name])
				s_scenes[s_current_name]->on_exit();
		}

		void resize(int width, int height) {
			if (!s_current_name.empty() && s_scenes[s_current_name])
				s_scenes[s_current_name]->on_resize(width, height);
		}

		const std::string& current_name() {
			return s_current_name;
		}

		Scene* current() {
			return s_scenes.count(s_current_name) ? s_scenes[s_current_name] : nullptr;
		}
	}

	// ===================================================================
	// SCENE SERIALIZATION (JSON SAVE/LOAD)
	// ===================================================================

	bool Scene::save_to_file() const {
		const char* filename = get_file();
		if (!filename || !*filename) return false;

		json root; root["entities"] = json::array();
		auto& reg = me::get_registry();
		auto& transforms = reg.view<me::components::TransformComponent>();

		for (size_t i = 0; i < transforms.size(); ++i) {
			me::entity::entity_id e = transforms.entity_map[i];
			auto& t = transforms.components[i];
			if (!reg.is_alive(e)) continue;

			json je;
			je["id"] = static_cast<uint32_t>(e);

			json comps = json::object();
			comps["Transform"] = json{ {"x", t.x}, {"y", t.y}, {"z", t.z}, {"rot_x", t.rot_x}, {"rot_y", t.rot_y}, {"rot_z", t.rot_z}, {"sx", t.sx}, {"sy", t.sy}, {"sz", t.sz} };

			if (auto* c = reg.try_get_component<me::components::Camera2DComponent>(e)) {
				comps["Camera2D"] = json{ {"offset_x", c->offset_x}, {"offset_y", c->offset_y}, {"zoom", c->zoom}, {"rotation", c->rotation} };
			}

			je["components"] = std::move(comps);
			root["entities"].push_back(std::move(je));
		}

		fs::path fullPath = fs::current_path() / "scenes" / filename;
		std::ofstream ofs(fullPath, std::ios::binary);
		if (!ofs) return false;
		ofs << root.dump(2);
		return true;
	}

	bool Scene::load_from_file() const {
		const char* filename = get_file();
		if (!filename || !*filename) return false;

		fs::path fullPath = fs::current_path() / "scenes" / filename;
		std::ifstream ifs(fullPath, std::ios::binary);
		if (!ifs) return false;

		json root;
		try { ifs >> root; } catch (...) { return false; }

		auto& reg = me::get_registry();
		if (!root.contains("entities") || !root["entities"].is_array()) return true;

		for (const auto& je : root["entities"]) {
			me::Entity e = reg.create_entity("Entity");
			if (!je.contains("components")) continue;
			const auto& comps = je["components"];

			if (comps.contains("Transform")) {
				auto& j = comps["Transform"];
				reg.add_component(e, me::components::TransformComponent{ j.value("x", 0.f), j.value("y", 0.f), j.value("z", 0.f), j.value("rot_x", 0.f), j.value("rot_y", 0.f), j.value("rot_z", 0.f), j.value("sx", 1.f), j.value("sy", 1.f), j.value("sz", 1.f) });
			}

			if (comps.contains("Camera2D")) {
				auto& j = comps["Camera2D"];
				reg.add_component(e, me::components::Camera2DComponent{ j.value("offset_x", 0.f), j.value("offset_y", 0.f), j.value("rotation", 0.f), j.value("zoom", 1.f), true });
			}
		}
		return true;
	}

} // namespace me