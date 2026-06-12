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
			me::get_registry().clear();
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
			auto& transforms = reg.view<me::components::TransformComponent>();
			for (size_t i = 0; i < transforms.size(); ++i) {
				me::entity::entity_id e = transforms.entity_map[i];
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

	} // namespace scene_manager
} // namespace me
