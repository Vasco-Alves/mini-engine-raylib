#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <sol/sol.hpp>

namespace me::components {

	// Holds the runtime data for a SINGLE script
	struct ScriptInstance {
		std::string path;
		bool started = false;
		sol::environment env;
		sol::protected_function update_fn; // Cache the function
		// Optional collision callbacks, dispatched after the physics step:
		//   on_collision_enter(self, other) / on_collision_exit(self, other)
		sol::protected_function on_collision_enter_fn;
		sol::protected_function on_collision_exit_fn;
		std::filesystem::file_time_type last_modified;
		// A script erroring in update would log at frame rate forever — the first
		// runtime error is logged, the rest are suppressed until a hot-reload.
		bool runtime_error_logged = false;
	};

	// The component attached to the entity, which holds ALL its scripts
	struct ScriptComponent {
		std::vector<ScriptInstance> scripts;
	};

}