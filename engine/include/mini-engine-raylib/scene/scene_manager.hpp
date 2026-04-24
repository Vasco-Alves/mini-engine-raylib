#pragma once

#include <string>

namespace me::scene {

	class GameScene; // forward declaration

	namespace manager {
		void register_scene(GameScene* scene);    // does NOT take ownership
		void load(const std::string& name);       // switches active scene
		void update(float dt);                    // calls current scene's on_update
		const std::string& current_name();        // current scene name
		GameScene* current();                     // pointer to current scene (or nullptr)
		bool save_current(); 				      // saves current scene back to its file
	}

} // namespace me::scene