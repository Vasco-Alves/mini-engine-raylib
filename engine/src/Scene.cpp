#include "Scene.hpp"
#include "Entity.hpp"
#include "Components.hpp"
#include "Assets.hpp"
// #include "Render2D.hpp" // REMOVED
#include "Registry.hpp"
#include "AssetsInternal.hpp"

#include <raylib.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using json = nlohmann::ordered_json;
namespace fs = std::filesystem;

namespace me::scene {

	// ---------- Helpers (Serialization Logic) ----------

	// [FIX] Transform (3D)
	static json ToJson(const me::components::Transform& t) {
		return json{
			{"x", t.x}, {"y", t.y}, {"z", t.z},
			{"rotX", t.rotX}, {"rotY", t.rotY}, {"rotZ", t.rotZ},
			{"sx", t.sx}, {"sy", t.sy}, {"sz", t.sz}
		};
	}

	static void FromJson(const json& j, me::components::Transform& t) {
		t.x = j.value("x", 0.0f);
		t.y = j.value("y", 0.0f);
		t.z = j.value("z", 0.0f);
		t.rotX = j.value("rotX", 0.0f);
		t.rotY = j.value("rotY", 0.0f);
		t.rotZ = j.value("rotZ", 0.0f);
		t.sx = j.value("sx", 1.0f);
		t.sy = j.value("sy", 1.0f);
		t.sz = j.value("sz", 1.0f);
	}

	// [FIX] Camera (3D)
	static json ToJson(const me::components::Camera& c) {
		return json{
			{"target", {c.targetX, c.targetY, c.targetZ}},
			{"up",     {c.upX, c.upY, c.upZ}},
			{"fov",    c.fov},
			{"proj",   c.projection},
			{"active", c.active}
		};
	}

	static void FromJson(const json& j, me::components::Camera& c) {
		if (j.contains("target")) {
			c.targetX = j["target"][0]; c.targetY = j["target"][1]; c.targetZ = j["target"][2];
		}
		if (j.contains("up")) {
			c.upX = j["up"][0]; c.upY = j["up"][1]; c.upZ = j["up"][2];
		}
		c.fov = j.value("fov", 60.0f);
		c.projection = j.value("proj", 0);
		c.active = j.value("active", false);
	}

	// [FIX] MeshRenderer
	static json ToJson(const me::components::MeshRenderer& m) {
		return json{
			{"type", (int)m.type},
			{"color", { m.color.r, m.color.g, m.color.b, m.color.a }},
			{"wire", m.wireframe}
		};
	}

	static void FromJson(const json& j, me::components::MeshRenderer& m) {
		m.type = (me::components::MeshRenderer::Type)j.value("type", 0);
		m.wireframe = j.value("wire", false);
		if (j.contains("color")) {
			m.color.r = j["color"][0]; m.color.g = j["color"][1];
			m.color.b = j["color"][2]; m.color.a = j["color"][3];
		}
	}

	// ---------- Save ----------
	bool Save(const char* filename) {
		if (!filename || !*filename) return false;

		json root;
		root["entities"] = json::array();

		auto& reg = me::detail::Reg();

		for (const auto& kv : reg.entities) {
			me::EntityId e = kv.first;
			if (!kv.second.alive) continue;

			json je;
			je["id"] = static_cast<uint32_t>(e);
			je["name"] = me::GetName(e);

			json comps = json::object();

			if (auto* c = reg.TryGetComponent<me::components::Transform>(e)) {
				comps["Transform"] = ToJson(*c);
			}
			if (auto* c = reg.TryGetComponent<me::components::MeshRenderer>(e)) {
				comps["MeshRenderer"] = ToJson(*c);
			}
			if (auto* c = reg.TryGetComponent<me::components::Camera>(e)) {
				comps["Camera"] = ToJson(*c);
			}
			// Add Physics/BoxCollider here later

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

	// ---------- Load ----------
	bool Load(const char* filename) {
		if (!filename || !*filename) return false;

		fs::path fullPath = fs::current_path() / "scenes" / filename;
		std::ifstream ifs(fullPath, std::ios::binary);
		if (!ifs) return false;

		json root;
		try { ifs >> root; } catch (...) { return false; }

		me::DestroyAllEntities();

		if (!root.contains("entities") || !root["entities"].is_array()) return true;

		for (const auto& je : root["entities"]) {
			me::Entity e = me::Entity::Create();

			if (je.contains("name") && je["name"].is_string())
				me::SetName(e.Id(), je["name"].get<std::string>().c_str());

			if (!je.contains("components")) continue;
			const auto& comps = je["components"];

			if (comps.contains("Transform")) {
				me::components::Transform t{};
				FromJson(comps["Transform"], t);
				// Overwrite default transform
				if (auto* existing = e.TryGet<me::components::Transform>()) *existing = t;
				else e.Add(t);
			}

			if (comps.contains("MeshRenderer")) {
				me::components::MeshRenderer m{};
				FromJson(comps["MeshRenderer"], m);
				e.Add(m);
			}

			if (comps.contains("Camera")) {
				me::components::Camera c{};
				FromJson(comps["Camera"], c);
				e.Add(c);
			}
		}
		return true;
	}
}