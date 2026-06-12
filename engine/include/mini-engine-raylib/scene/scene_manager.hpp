#pragma once

#include <string>

namespace me {

	class Registry;

	namespace scene_manager {

		// Wipes the global ECS clean
		void clear();

		// --- Operate on the global registry (me::get_registry()) ---

		// Reads a JSON file and populates the ECS
		bool load(const std::string& filepath);

		// Takes the current ECS and writes it to a JSON file
		bool save(const std::string& filepath);

		// --- Operate on an explicit registry (decoupled from global state; testable) ---

		bool load(Registry& reg, const std::string& filepath);
		bool save(Registry& reg, const std::string& filepath);

	}
}
