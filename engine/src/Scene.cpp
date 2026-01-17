#include "Scene.hpp"
#include "Entity.hpp"
#include "Components.hpp"
#include "Assets.hpp"
#include "Render2D.hpp"
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

	// ---------- helpers ----------

	// Transform2D
	static json ToJson(const me::components::Transform2D& t) {
		return json{
			{"x", t.x}, {"y", t.y},
			{"rotation", t.rotation},
			{"sx", t.sx}, {"sy", t.sy}
		};
	}

	static void FromJson(const json& j, me::components::Transform2D& t) {
		t.x = j.value("x", 0.0f);
		t.y = j.value("y", 0.0f);
		t.rotation = j.value("rotation", 0.0f);
		t.sx = j.value("sx", 1.0f);
		t.sy = j.value("sy", 1.0f);
	}

	// SpriteRenderer
	static json ToJson(const me::components::SpriteRenderer& s) {
		const char* uri = me::assets::ME_InternalGetTexturePath(s.tex);
		json j{
			{"layer",   s.layer},
			{"originX", s.originX}, {"originY", s.originY},
			{"flipX",   s.flipX},   {"flipY",   s.flipY},
			{"tint", { s.tint.r, s.tint.g, s.tint.b, s.tint.a }}
		};
		if (uri && *uri) j["tex"] = uri; // store URI if available
		return j;
	}

	static void FromJson(const json& j, me::components::SpriteRenderer& s) {
		if (j.contains("tex") && j["tex"].is_string()) {
			auto tex = me::assets::LoadTexture(j["tex"].get<std::string>().c_str());
			s.tex = tex;
		}
		s.layer = j.value("layer", 0);
		s.originX = j.value("originX", 0.0f);
		s.originY = j.value("originY", 0.0f);
		s.flipX = j.value("flipX", false);
		s.flipY = j.value("flipY", false);
		if (j.contains("tint") && j["tint"].is_array() && j["tint"].size() == 4) {
			s.tint.r = j["tint"][0].get<uint8_t>();
			s.tint.g = j["tint"][1].get<uint8_t>();
			s.tint.b = j["tint"][2].get<uint8_t>();
			s.tint.a = j["tint"][3].get<uint8_t>();
		}
	}

	// Camera2D
	static json ToJson(const me::components::Camera2D& c) {
		return json{
			{"x", c.x}, {"y", c.y},
			{"zoom", c.zoom},
			{"rotation", c.rotation},
			{"active", c.active}
		};
	}

	static void FromJson(const json& j, me::components::Camera2D& c) {
		c.x = j.value("x", 0.0f);
		c.y = j.value("y", 0.0f);
		c.zoom = j.value("zoom", 1.0f);
		c.rotation = j.value("rotation", 0.0f);
		c.active = j.value("active", false);
	}

	// Velocity2D
	static json ToJson(const me::components::Velocity2D& v) {
		return json{
			{"vx", v.vx},
			{"vy", v.vy}
		};
	}

	static void FromJson(const json& j, me::components::Velocity2D& v) {
		v.vx = j.value("vx", 0.0f);
		v.vy = j.value("vy", 0.0f);
	}

	// AabbCollider
	static json ToJson(const me::components::AabbCollider& c) {
		return json{
			{"w", c.w}, {"h", c.h},
			{"ox", c.ox}, {"oy", c.oy},
			{"solid", c.solid}
		};
	}

	static void FromJson(const json& j, me::components::AabbCollider& c) {
		c.w = j.value("w", 32.0f);
		c.h = j.value("h", 32.0f);
		c.ox = j.value("ox", 0.0f);
		c.oy = j.value("oy", 0.0f);
		c.solid = j.value("solid", true);
	}

	// SpriteSheet
	static json ToJson(const me::components::SpriteSheet& ss) {
		const char* uri = me::assets::ME_InternalGetTexturePath(ss.tex);
		json j{
			{"frameW",    ss.frameW},
			{"frameH",    ss.frameH},
			{"startIndex",ss.startIndex},
			{"frameCount",ss.frameCount},
			{"margin",    ss.margin},
			{"spacing",   ss.spacing},
			{"cols",      ss.cols}
		};
		if (uri && *uri) j["tex"] = uri;
		return j;
	}

	static void FromJson(const json& j, me::components::SpriteSheet& ss) {
		if (j.contains("tex") && j["tex"].is_string()) {
			auto tex = me::assets::LoadTexture(j["tex"].get<std::string>().c_str());
			ss.tex = tex;
		}
		ss.frameW = j.value("frameW", 0);
		ss.frameH = j.value("frameH", 0);
		ss.startIndex = j.value("startIndex", 0);
		ss.frameCount = j.value("frameCount", 0);
		ss.margin = j.value("margin", 0);
		ss.spacing = j.value("spacing", 0);
		ss.cols = j.value("cols", 0);
	}

	// AnimationPlayer
	static json ToJson(const me::components::AnimationPlayer& ap) {
		return json{
			{"current",   ap.current},
			{"fps",       ap.fps},
			{"loop",      ap.loop},
			{"playing",   ap.playing},
			{"timeAccum", ap.timeAccum}
		};
	}

	static void FromJson(const json& j, me::components::AnimationPlayer& ap) {
		ap.current = j.value("current", 0);
		ap.fps = j.value("fps", 8.0f);
		ap.loop = j.value("loop", true);
		ap.playing = j.value("playing", true);
		ap.timeAccum = j.value("timeAccum", 0.0f);
	}

	// ---------- Save ----------
	bool Save(const char* filename) {
		if (!filename || !*filename) return false;

		json root;
		root["entities"] = json::array();

		// Iterate entities via registry
		me::detail::Reg().ForEachEntity([&](me::EntityId e) {
			json je;
			je["id"] = static_cast<uint32_t>(e);
			je["name"] = me::GetName(e);

			json comps = json::object();

			me::components::Transform2D t{};
			if (me::GetComponent(e, t)) {
				comps["Transform2D"] = ToJson(t);
			}

			me::components::SpriteRenderer sr{};
			if (me::GetComponent(e, sr)) {
				comps["SpriteRenderer"] = ToJson(sr);
			}

			me::components::Camera2D cam{};
			if (me::GetComponent(e, cam)) {
				comps["Camera2D"] = ToJson(cam);
			}

			me::components::Velocity2D v{};
			if (me::GetComponent(e, v)) {
				comps["Velocity2D"] = ToJson(v);
			}

			me::components::AabbCollider col{};
			if (me::GetComponent(e, col)) {
				comps["AabbCollider"] = ToJson(col);
			}

			me::components::SpriteSheet ss{};
			if (me::GetComponent(e, ss)) {
				comps["SpriteSheet"] = ToJson(ss);
			}

			me::components::AnimationPlayer ap{};
			if (me::GetComponent(e, ap)) {
				comps["AnimationPlayer"] = ToJson(ap);
			}

			je["components"] = std::move(comps);
			root["entities"].push_back(std::move(je));
			});

		// <executable_dir>/scenes/<filename>
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
		try {
			ifs >> root;
		} catch (...) {
			return false;
		}

		// Clear current world
		me::DestroyAllEntities();

		if (!root.contains("entities") || !root["entities"].is_array())
			return true; // empty scene

		for (const auto& je : root["entities"]) {
			me::Entity e = me::Entity::Create();

			if (je.contains("name") && je["name"].is_string())
				me::SetName(e.Id(), je["name"].get<std::string>().c_str());

			if (!je.contains("components")) continue;
			const auto& comps = je["components"];

			if (comps.contains("Transform2D")) {
				me::components::Transform2D t{};
				FromJson(comps["Transform2D"], t);
				e.Add(t);
			}

			if (comps.contains("SpriteRenderer")) {
				me::components::SpriteRenderer sr{};
				FromJson(comps["SpriteRenderer"], sr);
				e.Add(sr);
			}

			if (comps.contains("Camera2D")) {
				me::components::Camera2D cam{};
				FromJson(comps["Camera2D"], cam);
				e.Add(cam);
				if (cam.active) {
					me::render2d::SetActiveCamera(e.Id());
				}
			}

			if (comps.contains("Velocity2D")) {
				me::components::Velocity2D v{};
				FromJson(comps["Velocity2D"], v);
				e.Add(v);
			}

			if (comps.contains("AabbCollider")) {
				me::components::AabbCollider col{};
				FromJson(comps["AabbCollider"], col);
				e.Add(col);
			}

			if (comps.contains("SpriteSheet")) {
				me::components::SpriteSheet ss{};
				FromJson(comps["SpriteSheet"], ss);
				e.Add(ss);
			}

			if (comps.contains("AnimationPlayer")) {
				me::components::AnimationPlayer ap{};
				FromJson(comps["AnimationPlayer"], ap);
				e.Add(ap);
			}
		}
		return true;
	}
}
