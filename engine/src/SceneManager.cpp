#include "SceneManager.hpp"
#include "GameScene.hpp"
#include "Scene.hpp"
#include "Entity.hpp"

#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

namespace me::scene::manager {

	namespace {
		std::unordered_map<std::string, GameScene*> s_Scenes;
		std::string s_CurrentName;
	}

	void Register(GameScene* scene) {
		if (!scene) return;
		std::string name = scene->GetName();
		if (name.empty()) {
			std::cerr << "Scene registered with empty name\n";
			return;
		}

		s_Scenes[name] = scene;

		// Auto-create scene file with default camera if file is specified
		const char* file = scene->GetFile();
		if (file && *file) {
			fs::path sceneFolder = fs::current_path() / "scenes";
			fs::create_directories(sceneFolder);

			fs::path fullPath = sceneFolder / file;
			if (!fs::exists(fullPath)) {
				std::ofstream out(fullPath, std::ios::binary);
				if (out) {
					out << R"({
  "entities": [
    {
      "name": "Camera",
      "components": {
        "Camera2D": {
          "x": 0.0,
          "y": 0.0,
          "zoom": 1.0,
          "rotation": 0.0,
          "active": true
        }
      }
    }
  ]
}
)";
					std::cout << "Created new scene file with default camera: "
						<< fullPath.string() << "\n";
				} else {
					std::cerr << "Failed to create scene file: "
						<< fullPath.string() << "\n";
				}
			}
		}

		scene->OnRegister();
	}

	void Load(const std::string& name) {
		// call OnExit on current scene
		if (!s_CurrentName.empty()) {
			auto itOld = s_Scenes.find(s_CurrentName);
			if (itOld != s_Scenes.end() && itOld->second)
				itOld->second->OnExit();
		}

		auto it = s_Scenes.find(name);
		if (it == s_Scenes.end()) {
			std::cerr << "Scene not registered: " << name << "\n";
			return;
		}

		GameScene* scene = it->second;

		const char* file = scene->GetFile();
		if (file && *file) {
			if (!me::scene::Load(file)) {
				std::cerr << "Failed to load scene file: " << file << "\n";
				return;
			}
		} else {
			// code-only scene: clear world, let OnEnter populate it
			me::DestroyAllEntities();
		}

		s_CurrentName = name;
		std::cout << "Scene loaded: " << s_CurrentName << "\n";

		scene->OnEnter();
	}

	void Update(float dt) {
		if (s_CurrentName.empty()) return;
		auto it = s_Scenes.find(s_CurrentName);
		if (it == s_Scenes.end() || !it->second) return;
		it->second->OnUpdate(dt);
	}

	const std::string& CurrentName() {
		return s_CurrentName;
	}

	GameScene* Current() {
		auto it = s_Scenes.find(s_CurrentName);
		if (it == s_Scenes.end()) return nullptr;
		return it->second;
	}

	bool SaveCurrent() {
		GameScene* scene = Current();
		if (!scene) return false;
		return scene->SaveSelf();
	}

} // namespace me::scene::manager
