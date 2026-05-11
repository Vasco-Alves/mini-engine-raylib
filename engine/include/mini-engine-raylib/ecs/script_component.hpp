#pragma once

#include <string>

#include <sol/sol.hpp>

namespace me::components {

	struct ScriptComponent {
		std::string path;
		bool started = false;

		sol::environment env;
		sol::protected_function update_fn; // Cache the function
	};

} // namespace me::components