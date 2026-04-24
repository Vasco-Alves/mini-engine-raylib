#include "mini-engine-raylib/scene/scene_manager.hpp"
#include "mini-engine-raylib/scene/scene.hpp"
#include "mini-engine-raylib/scene/game_scene.hpp"
#include "mini-engine-raylib/core/engine.hpp" // Needs to provide a reset_registry() or similar in the future

#include <mini-ecs/registry.hpp>

#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

namespace me::scene::manager {

	namespace {
		std::unordered_map<std::string, GameScene*> s_scenes;
		std::string s_current_name;
	}

	void register_scene(GameScene* scene) {
		if (!scene) return;
		std::string name = scene->get_name();
		if (name.empty()) {
			std::cerr << "Scene registered with empty name\n";
			return;
		}

		s_scenes[name] = scene;

		const char* file = scene->get_file();
		if (file && *file) {
			fs::path scene_folder = fs::current_path() / "scenes";
			fs::create_directories(scene_folder);

			fs::path full_path = scene_folder / file;
			if (!fs::exists(full_path)) {
				std::ofstream out(full_path, std::ios::binary);
				if (out) {
					out << R"({ "entities": [] })";
					std::cout << "Created new scene file: " << full_path.string() << "\n";
				}
			}
		}

		scene->on_register();
	}

	void load(const std::string& name) {
		if (!s_current_name.empty()) {
			auto it_old = s_scenes.find(s_current_name);
			if (it_old != s_scenes.end() && it_old->second)
				it_old->second->on_exit();
		}

		auto it = s_scenes.find(name);
		if (it == s_scenes.end()) {
			std::cerr << "Scene not registered: " << name << "\n";
			return;
		}

		GameScene* scene = it->second;

		const char* file = scene->get_file();
		if (file && *file) {
			// Calls me::scene::load from scene.hpp/.cpp
			if (!me::scene::load(file)) {
				std::cerr << "Failed to load scene file: " << file << "\n";
				return;
			}
		} else {
			// This will likely be mapped to me::get_registry().clear() in your final engine pass
			// me::reset_registry(); 
		}

		s_current_name = name;
		std::cout << "Scene loaded: " << s_current_name << "\n";

		scene->on_enter();
	}

	void update(float dt) {
		if (s_current_name.empty()) return;
		auto it = s_scenes.find(s_current_name);
		if (it == s_scenes.end() || !it->second) return;
		it->second->on_update(dt);
	}

	const std::string& current_name() {
		return s_current_name;
	}

	GameScene* current() {
		auto it = s_scenes.find(s_current_name);
		if (it == s_scenes.end()) return nullptr;
		return it->second;
	}

	bool save_current() {
		GameScene* scene = current();
		if (!scene) return false;
		return scene->save_self();
	}
}