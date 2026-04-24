#include "mini-engine-raylib/scene/scene.hpp"
#include "mini-engine-raylib/ecs/components.hpp"
#include "mini-engine-raylib/core/engine.hpp"   

#include <raylib.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using json = nlohmann::ordered_json;
namespace fs = std::filesystem;

namespace me::scene {

	static json to_json(const me::components::TransformComponent& t) {
		return json{
			{"x", t.x}, {"y", t.y}, {"z", t.z},
			{"rot_x", t.rot_x}, {"rot_y", t.rot_y}, {"rot_z", t.rot_z},
			{"sx", t.sx}, {"sy", t.sy}, {"sz", t.sz}
		};
	}

	static void from_json(const json& j, me::components::TransformComponent& t) {
		t.x = j.value("x", 0.0f); t.y = j.value("y", 0.0f); t.z = j.value("z", 0.0f);
		t.rot_x = j.value("rot_x", 0.0f); t.rot_y = j.value("rot_y", 0.0f); t.rot_z = j.value("rot_z", 0.0f);
		t.sx = j.value("sx", 1.0f); t.sy = j.value("sy", 1.0f); t.sz = j.value("sz", 1.0f);
	}

	static json to_json(const me::components::CameraComponent& c) {
		return json{
			{"target", {c.target_x, c.target_y, c.target_z}},
			{"up",     {c.up_x, c.up_y, c.up_z}},
			{"fov",    c.fov},
			{"proj",   c.projection},
			{"active", c.active}
		};
	}

	static void from_json(const json& j, me::components::CameraComponent& c) {
		if (j.contains("target")) { c.target_x = j["target"][0]; c.target_y = j["target"][1]; c.target_z = j["target"][2]; }
		if (j.contains("up")) { c.up_x = j["up"][0]; c.up_y = j["up"][1]; c.up_z = j["up"][2]; }
		c.fov = j.value("fov", 60.0f);
		c.projection = j.value("proj", 0);
		c.active = j.value("active", false);
	}

	static json to_json(const me::components::MeshRendererComponent& m) {
		return json{
			{"type", (int)m.type},
			{"color", { m.color.r, m.color.g, m.color.b, m.color.a }},
			{"wire", m.wireframe}
		};
	}

	static void from_json(const json& j, me::components::MeshRendererComponent& m) {
		m.type = (me::components::MeshRendererComponent::Type)j.value("type", 0);
		m.wireframe = j.value("wire", false);
		if (j.contains("color")) {
			m.color.r = j["color"][0]; m.color.g = j["color"][1];
			m.color.b = j["color"][2]; m.color.a = j["color"][3];
		}
	}

	bool save(const char* filename) {
		if (!filename || !*filename) return false;

		json root;
		root["entities"] = json::array();

		auto& reg = me::get_registry();

		auto& transforms = reg.view<me::components::TransformComponent>();

		for (size_t i = 0; i < transforms.size(); ++i) {
			me::EntityId e = transforms.entity_map[i];
			auto& t = transforms.components[i];

			if (!reg.is_alive(e)) continue;

			json je;
			je["id"] = static_cast<uint32_t>(e);
			je["name"] = "Entity";

			json comps = json::object();
			comps["Transform"] = to_json(t);

			if (auto* c = reg.try_get_component<me::components::MeshRendererComponent>(e)) {
				comps["MeshRendererComponent"] = to_json(*c);
			}
			if (auto* c = reg.try_get_component<me::components::CameraComponent>(e)) {
				comps["Camera"] = to_json(*c);
			}

			je["components"] = std::move(comps);
			root["entities"].push_back(std::move(je));
		}

		fs::path sceneDir = fs::current_path() / "scenes";
		fs::create_directories(sceneDir);
		fs::path fullPath = sceneDir / filename;

		std::ofstream ofs(fullPath, std::ios::binary);
		if (!ofs) return false;
		ofs << root.dump(2);
		return true;
	}

	bool load(const char* filename) {
		if (!filename || !*filename) return false;

		fs::path fullPath = fs::current_path() / "scenes" / filename;
		std::ifstream ifs(fullPath, std::ios::binary);
		if (!ifs) return false;

		json root;
		try { ifs >> root; } catch (...) { return false; }

		// NOTE: Make sure to implement me::reset_registry() or similar in your engine code
		// if it was previously me::ResetRegistry().
		// me::reset_registry(); 
		auto& reg = me::get_registry();

		if (!root.contains("entities") || !root["entities"].is_array()) return true;

		for (const auto& je : root["entities"]) {
			std::string name = je.value("name", "Entity");
			me::Entity e = reg.create_entity(name);

			if (!je.contains("components")) continue;
			const auto& comps = je["components"];

			if (comps.contains("Transform")) {
				me::components::TransformComponent t{};
				from_json(comps["Transform"], t);
				reg.add_component(e, t);
			}

			if (comps.contains("MeshRendererComponent")) {
				me::components::MeshRendererComponent m{};
				from_json(comps["MeshRendererComponent"], m);
				reg.add_component(e, m);
			}

			if (comps.contains("Camera")) {
				me::components::CameraComponent c{};
				from_json(comps["Camera"], c);
				reg.add_component(e, c);
			}
		}
		return true;
	}
}