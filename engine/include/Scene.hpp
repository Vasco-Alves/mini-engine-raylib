#pragma once

#include <cstdint>

namespace me::scene {
	// Save all entities + known components to JSON at 'path'.
	// Returns true on success.
	bool Save(const char* path);

	// Clear world, then load entities + components from JSON at 'path'.
	// Returns true on success.
	bool Load(const char* path);
}
