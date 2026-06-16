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

		// --- Deferred scene switching (gameplay: Lua's Scene.load) ---
		// Scripts run in the middle of the frame, while systems are iterating the
		// registry — swapping the scene there would pull the rug out from under
		// them. request_load parks the path; the engine loop applies it at the end
		// of the frame (rebuilding the physics world when the simulation is live).
		// A second request in the same frame replaces the first.

		void request_load(const std::string& filepath);
		bool has_pending_load();

		// Applies (and clears) the pending request on the global registry.
		bool apply_pending_load();

	}
}
