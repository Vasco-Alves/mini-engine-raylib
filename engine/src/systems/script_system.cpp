#include "core/engine.hpp"
#include "systems/script_system.hpp"
#include "ecs/script_component.hpp"
#include "scripting/script_manager.hpp"

#include <mini-ecs/registry.hpp>

#include <iostream>

namespace me::systems {

	void script_update(float dt) {
		auto& reg = me::get_registry();

		auto& pool = reg.view<me::components::ScriptComponent>();
		sol::state& lua = me::scripting::get_state();

		for (size_t i = 0; i < pool.size(); ++i) {
			auto e = pool.entity_map[i];
			auto& script = pool.components[i];

			// 1. Initialization (Run Once)
			if (!script.started) {
				// Create an isolated environment backed by the global Lua state
				script.env = sol::environment(lua, sol::create, lua.globals());

				try {
					// Load the script into this specific environment
					lua.script_file(script.path, script.env);

					// Call start() if it exists
					sol::protected_function start_fn = script.env["start"];
					if (start_fn.valid()) {
						start_fn(e);
					}
				} catch (const sol::error& err) {
					std::cerr << "[Lua Error] Failed to load " << script.path << "\n" << err.what() << "\n";
				}
				script.started = true;
			}

			// 2. Update (Run Every Frame)
			sol::protected_function update_fn = script.env["update"];
			if (update_fn.valid()) {
				auto result = update_fn(e, dt);
				if (!result.valid()) {
					sol::error err = result;
					std::cerr << "[Lua Error] " << err.what() << "\n";
				}
			}
		}
	}
}