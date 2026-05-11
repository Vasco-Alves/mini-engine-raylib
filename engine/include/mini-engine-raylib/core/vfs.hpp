#pragma once

#include <string>

namespace me::vfs {

	// Mounts a physical directory to a virtual prefix.
	// e.g., mount("engine", "assets") allows "engine://icons/file.png"
	void mount(const std::string& scheme, const std::string& physical_path);

	// Resolves a virtual path to a physical path for Raylib to load.
	// e.g., resolve("game://models/car.glb") -> "C:/Dev/MyGame/assets/models/car.glb"
	std::string resolve(const std::string& virtual_path);

} // namespace me::vfs
