#include "mini-engine-raylib/systems/script_system.hpp"

#include <iostream>
#include <filesystem>
#include <unordered_map>

#include "mini-engine-raylib/core/engine.hpp"
#include "mini-engine-raylib/ecs/script_component.hpp"
#include "mini-engine-raylib/scripting/script_manager.hpp"
#include "mini-engine-raylib/core/vfs.hpp" 
#include "mini-engine-raylib/core/logger.hpp"
#include <mini-ecs/registry.hpp>

namespace me::systems {

	static float s_FileCheckTimer = 0.0f;

	void script_update(float dt) {
		auto& reg = me::get_registry();
		auto& pool = reg.view<me::components::ScriptComponent>();
		sol::state& lua = me::scripting::get_state();

		s_FileCheckTimer += dt;
		bool should_check_files = false;
		if (s_FileCheckTimer >= 0.5f) {
			should_check_files = true;
			s_FileCheckTimer = 0.0f;
		}

		// Only allocate the map if checking files this frame
		std::unordered_map<std::string, std::filesystem::file_time_type> cached_timestamps;

		for (int i = (int)pool.size() - 1; i >= 0; --i) {
			auto e = pool.entity_map[i];
			auto& script_comp = pool.components[i];

			for (auto& script : script_comp.scripts) {

				// ==========================================
				// 1. FIRST-TIME INITIALIZATION
				// ==========================================
				if (!script.started) {
					std::string physical_path = me::vfs::resolve(script.path);

					// ONLY hit the hard drive if we are initializing it for the first time
					if (!std::filesystem::exists(physical_path)) continue;

					script.env = sol::environment(lua, sol::create, lua.globals());

					try {
						lua.script_file(physical_path, script.env);

						sol::protected_function start_fn = script.env["start"];
						if (start_fn.valid()) {
							start_fn(me::Entity{ e, &reg });
						}

						script.update_fn = script.env["update"];
						script.last_modified = std::filesystem::last_write_time(physical_path);

					} catch (const sol::error& err) {
						me::logger::error("Failed to load " + script.path + ":\n" + err.what());
					}
					script.started = true;
				}

				// ==========================================
				// 2. HOT-RELOAD LOGIC
				// ==========================================
				if (script.started && should_check_files) {

					std::string physical_path = me::vfs::resolve(script.path);

					// ONLY hit the hard drive if the 0.5s timer told us to check for updates
					if (!std::filesystem::exists(physical_path)) continue;

					std::filesystem::file_time_type current_time;

					if (cached_timestamps.find(physical_path) != cached_timestamps.end()) {
						current_time = cached_timestamps[physical_path];
					} else {
						current_time = std::filesystem::last_write_time(physical_path);
						cached_timestamps[physical_path] = current_time;
					}

					if (current_time > script.last_modified) {
						me::logger::info("Hot-Reloading: " + script.path);

						try {
							lua.script_file(physical_path, script.env);
							script.update_fn = script.env["update"];
							script.last_modified = current_time;
						} catch (const sol::error& err) {
							me::logger::error("Syntax Error in " + script.path + ":\n" + err.what());
							script.last_modified = current_time;
						}
					}
				}

				// ==========================================
				// 3. THE ACTUAL UPDATE LOOP
				// ==========================================
				// Notice how this loop does ZERO string comparisons and ZERO disk reads!
				if (script.started && script.update_fn.valid()) {
					auto result = script.update_fn(me::Entity{ e, &reg }, dt);

					if (!result.valid()) {
						sol::error err = result;
						me::logger::error("Runtime Error in " + script.path + ":\n" + err.what());
					}
				}
			}
		}
	}
}