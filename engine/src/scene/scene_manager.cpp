#include "mini-engine-raylib/scene/scene_manager.hpp"

#include <fstream>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>

#include "mini-engine-raylib/core/engine.hpp"
#include "mini-engine-raylib/core/events.hpp"
#include "mini-engine-raylib/core/logger.hpp"
#include "mini-engine-raylib/core/file_system.hpp"
#include "mini-engine-raylib/ecs/components.hpp"
#include "mini-engine-raylib/ecs/component_registry.hpp"
#include <mini-ecs/registry.hpp>

using json = nlohmann::ordered_json;

namespace me {
	namespace scene_manager {

		void clear() {
			// Same rule as load(): the outgoing entities must release their
			// ref-counted asset/audio handles before the pools are wiped.
			auto& reg = me::get_registry();
			for (auto [e, t] : reg.view<me::components::TransformComponent>()) {
				(void)t;
				me::ecs::release_native_handles(reg, e);
			}
			reg.clear();
		}

		// =====================================================================
		// SAVE
		// =====================================================================
		// Bump when the on-disk scene structure changes incompatibly; loaders
		// can then migrate or at least warn instead of silently misreading.
		constexpr int kSceneFormatVersion = 1;

		bool save(Registry& reg, const std::string& filepath) {
			json root;
			root["version"] = kSceneFormatVersion;
			root["entities"] = json::array();

			// Every persisted entity carries a Transform, so the transform pool is
			// our entity enumeration. Each present component is written by the registry.
			for (auto [e, t] : reg.view<me::components::TransformComponent>()) {
				(void)t;
				if (!reg.is_alive(e)) continue;

				json je;
				je["id"] = static_cast<uint32_t>(e);

				json comps = json::object();
				for (const auto& meta : me::ecs::components()) {
					if (meta.has(reg, e)) comps[meta.name] = meta.save(reg, e);
				}

				je["components"] = std::move(comps);
				root["entities"].push_back(std::move(je));
			}

			std::filesystem::path physical_path = me::fs::resolve_if_virtual(filepath);

			std::ofstream ofs(physical_path, std::ios::binary);
			if (!ofs) {
				me::logger::error("Failed to open scene file for writing: " + physical_path.string());
				return false;
			}
			try {
				ofs.exceptions(std::ios::failbit | std::ios::badbit);
				ofs << root.dump(4);
			} catch (const std::exception& ex) {
				me::logger::error("Error writing scene file: " + std::string(ex.what()));
				return false;
			}
			return true;
		}

		// =====================================================================
		// LOAD
		// =====================================================================
		bool load(Registry& reg, const std::string& filepath) {
			std::filesystem::path physical_path = me::fs::resolve_if_virtual(filepath);

			std::ifstream ifs(physical_path, std::ios::binary);
			if (!ifs) {
				me::logger::error("Failed to open scene file: " + physical_path.string());
				return false;
			}

			json root;
			try {
				ifs >> root;
			} catch (const std::exception& ex) {
				me::logger::error("Failed to parse scene JSON: " + std::string(ex.what()));
				return false;
			}

			int version = root.value("version", 1); // pre-versioning files are format 1
			if (version > kSceneFormatVersion) {
				me::logger::warn("Scene was saved by a newer engine (format " + std::to_string(version)
					+ ", this build reads " + std::to_string(kSceneFormatVersion) + ") - loading anyway.");
			}

			// The asset/audio caches are ref-counted and clear() doesn't run the
			// deletion pipeline, so the outgoing scene must release its native
			// handles here — otherwise every scene switch pins the old scene's
			// models/sounds in memory for the rest of the session. The transform
			// pool enumerates every persisted entity (same convention as save()).
			for (auto [e, t] : reg.view<me::components::TransformComponent>()) {
				(void)t;
				me::ecs::release_native_handles(reg, e);
			}

			reg.clear();

			if (!root.contains("entities") || !root["entities"].is_array()) return true;

			// -----------------------------------------------------------------
			// PASS 1: create every entity and map old ids -> new ids
			// -----------------------------------------------------------------
			std::unordered_map<uint32_t, me::entity::entity_id> old_to_new;
			std::vector<me::Entity> loaded;
			loaded.reserve(root["entities"].size());

			for (const auto& je : root["entities"]) {
				me::Entity e = reg.create_entity();
				loaded.push_back(e);

				uint32_t old_id = je.value("id", 0u);
				if (old_id != 0) old_to_new[old_id] = e.get_id();
			}

			// -----------------------------------------------------------------
			// PASS 2: load each entity's components via the registry
			// -----------------------------------------------------------------
			for (size_t i = 0; i < root["entities"].size(); ++i) {
				const auto& je = root["entities"][i];
				if (!je.contains("components")) continue;
				const auto& comps = je["components"];
				me::entity::entity_id e = loaded[i].get_id();

				for (const auto& meta : me::ecs::components()) {
					if (!comps.contains(meta.name)) continue;
					// One malformed component must not abort the whole scene:
					// log it, skip it, keep loading everything else.
					try {
						meta.load(reg, e, comps[meta.name]);
					} catch (const std::exception& ex) {
						me::logger::warn("Scene load: skipping malformed '" + meta.name
							+ "' component: " + ex.what());
					}
				}
			}

			// -----------------------------------------------------------------
			// PASS 3: re-link the Transform hierarchy now that all ids are known
			// -----------------------------------------------------------------
			for (size_t i = 0; i < root["entities"].size(); ++i) {
				const auto& je = root["entities"][i];
				if (!je.contains("components")) continue;
				const auto& comps = je["components"];
				if (!comps.contains("Transform")) continue;

				auto* t = reg.try_get_component<me::components::TransformComponent>(loaded[i].get_id());
				if (!t) continue;
				const auto& jt = comps["Transform"];

				uint32_t old_parent = jt.value("parent", 0u);
				if (old_parent != 0) {
					auto it = old_to_new.find(old_parent);
					if (it != old_to_new.end()) {
						t->parent = it->second;
					} else {
						me::logger::warn("Scene load: entity references unknown parent ID " +
							std::to_string(old_parent) + " — hierarchy link dropped");
						t->parent = me::entity::null;
					}
				} else {
					t->parent = me::entity::null;
				}

				if (jt.contains("children") && jt["children"].is_array()) {
					for (uint32_t old_child : jt["children"]) {
						auto it = old_to_new.find(old_child);
						if (it != old_to_new.end()) {
							t->children.push_back(it->second);
						} else {
							me::logger::warn("Scene load: entity references unknown child ID " +
								std::to_string(old_child) + " — link dropped");
						}
					}
				}
			}

			me::get_event_bus().publish<me::events::SceneLoadedEvent>(filepath);
			return true;
		}

		// =====================================================================
		// GLOBAL-REGISTRY CONVENIENCE OVERLOADS
		// =====================================================================
		bool save(const std::string& filepath) { return save(me::get_registry(), filepath); }
		bool load(const std::string& filepath) { return load(me::get_registry(), filepath); }

		// =====================================================================
		// DEFERRED SCENE SWITCHING (applied by the engine loop at end of frame)
		// =====================================================================
		namespace {
			std::string s_pending_load;
		}

		void request_load(const std::string& filepath) {
			if (!s_pending_load.empty() && s_pending_load != filepath)
				me::logger::warn("Scene.load: replacing pending request '" + s_pending_load + "' with '" + filepath + "'");
			s_pending_load = filepath;
		}

		bool has_pending_load() {
			return !s_pending_load.empty();
		}

		bool apply_pending_load() {
			std::string path;
			std::swap(path, s_pending_load);
			if (path.empty()) return false;

			me::logger::info("Switching scene to: " + path);
			return load(path);
		}

	} // namespace scene_manager
} // namespace me
