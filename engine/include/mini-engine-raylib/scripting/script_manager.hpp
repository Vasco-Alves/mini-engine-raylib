#pragma once

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

namespace me::scripting {

	// Initializes the Lua state, opens standard libraries, and binds engine structs
	void init();

	// Cleans up the Lua state on exit
	void shutdown();

	// Registers all C++ Engine components, math, and tools so Lua can use them
	void bind_engine();

	// Returns a reference to the global Lua state
	sol::state& get_state();

} // namespace me::scripting