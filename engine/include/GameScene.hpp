#pragma once

#include "Scene.hpp"

namespace me::scene {

	class GameScene {
	public:
		virtual ~GameScene() = default;

		// Unique name used by the manager (e.g. "Base", "Space")
		virtual const char* GetName() const = 0;

		// JSON file for this scene, or "" / nullptr for code-only scenes
		virtual const char* GetFile() const { return ""; }

		// convenience: save this scene's world back to its file
		bool SaveSelf() const {
			const char* file = GetFile();
			return (file && *file) ? me::scene::Save(file) : false;
		}

		// Optional hooks
		virtual void OnRegister() {}       // called when registered
		virtual void OnEnter() {}          // called after world is loaded
		virtual void OnExit() {}           // called before switching away
		virtual void OnUpdate(float /*dt*/) {} // per-frame logic
	};

} // namespace me::scene
