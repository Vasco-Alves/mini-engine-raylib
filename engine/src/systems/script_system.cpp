#include "mini-engine-raylib/systems/script_system.hpp"

#include <iostream>
#include <filesystem>

#include "mini-engine-raylib/core/engine.hpp"
#include "mini-engine-raylib/ecs/script_component.hpp"
#include "mini-engine-raylib/scripting/script_manager.hpp"
#include "mini-engine-raylib/core/vfs.hpp" 
#include "mini-engine-raylib/core/logger.hpp"

#include <mini-ecs/registry.hpp>

namespace me::systems {

	// A static timer so we don't spam the hard drive every single frame
	static float s_FileCheckTimer = 0.0f;

	void script_update(float dt) {
		auto& reg = me::get_registry();
		auto& pool = reg.view<me::components::ScriptComponent>();
		sol::state& lua = me::scripting::get_state();

		// 1. Advance the polling timer
		s_FileCheckTimer += dt;
		bool should_check_files = false;
		if (s_FileCheckTimer >= 0.5f) { // Check every half-second
			should_check_files = true;
			s_FileCheckTimer = 0.0f;
		}

		// Iterate backwards. If a Lua script destroys an entity, the pool size shrinks.
		for (int i = (int)pool.size() - 1; i >= 0; --i) {
			auto e = pool.entity_map[i];
			auto& script_comp = pool.components[i];

			for (auto& script : script_comp.scripts) {

				std::string physical_path = me::vfs::resolve(script.path);

				// Make sure the file actually exists before touching it
				if (!std::filesystem::exists(physical_path)) continue;

				// ==========================================
				// 2. HOT-RELOAD LOGIC
				// ==========================================
				if (script.started && should_check_files) {
					auto current_time = std::filesystem::last_write_time(physical_path);

					// If the file on disk is newer than our memory timestamp...
					if (current_time > script.last_modified) {
						me::logger::info("Hot-Reloading: " + script.path);

						try {
							// Re-run the file in the exact same environment
							lua.script_file(physical_path, script.env);

							// Re-grab the functions in case the user changed their names/logic
							script.update_fn = script.env["update"];

							// Update our timestamp so we don't reload it again next frame!
							script.last_modified = current_time;
						} catch (const sol::error& err) {
							// If you made a syntax error in VS Code, we catch it gracefully 
							// so the engine doesn't crash, and print it to the UI!
							me::logger::error("Syntax Error in " + script.path + ":\n" + err.what());

							// We still update the timestamp so it stops trying to load the broken file
							script.last_modified = current_time;
						}
					}
				}

				// ==========================================
				// 3. FIRST-TIME INITIALIZATION
				// ==========================================
				if (!script.started) {
					script.env = sol::environment(lua, sol::create, lua.globals());

					try {
						lua.script_file(physical_path, script.env);

						sol::protected_function start_fn = script.env["start"];
						if (start_fn.valid()) {
							start_fn(me::Entity{ e, &reg });
						}

						script.update_fn = script.env["update"];

						// Save the timestamp of when we first loaded it
						script.last_modified = std::filesystem::last_write_time(physical_path);

					} catch (const sol::error& err) {
						me::logger::error("Failed to load " + script.path + ":\n" + err.what());
					}
					script.started = true;
				}

				// ==========================================
				// 4. THE ACTUAL UPDATE LOOP
				// ==========================================
				if (script.update_fn.valid()) {
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
