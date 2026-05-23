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
		std::filesystem::file_time_type last_modified;
	};

	// The component attached to the entity, which holds ALL its scripts
	struct ScriptComponent {
		std::vector<ScriptInstance> scripts;
	};

}