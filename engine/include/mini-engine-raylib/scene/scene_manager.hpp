#pragma once

#include <string>

namespace me {
	namespace scene_manager {

		// Wipes the ECS clean
		void clear();

		// Reads a JSON file and populates the ECS
		bool load(const std::string& filepath);

		// Takes the current ECS and writes it to a JSON file
		bool save(const std::string& filepath);

	}
}