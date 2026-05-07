#pragma once

#include <string>
#include <sol/sol.hpp>

namespace me::components {

	struct ScriptComponent {
		std::string path;
		bool started = false;

		sol::environment env;
	};

} // namespace me::components