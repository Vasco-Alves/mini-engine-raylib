#pragma once

#include <string>

namespace me::scene {

	class GameScene; // forward declaration

	namespace manager {
		void Register(GameScene* scene);          // does NOT take ownership
		void Load(const std::string& name);       // switches active scene
		void Update(float dt);                    // calls current scene's OnUpdate
		const std::string& CurrentName();         // current scene name
		GameScene* Current();                     // pointer to current scene (or nullptr)
		bool SaveCurrent(); 				      // saves current scene back to its file
	}

} // namespace me::scene
