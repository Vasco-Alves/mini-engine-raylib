#pragma once

#include <cstdint>

namespace me {

	// Base class for user-defined game applications.
	class GameApp {
	public:
		virtual ~GameApp() = default;

		// Called once at startup, after Init()
		virtual void OnStart() {}

		// Called every frame with delta time (in seconds)
		virtual void OnUpdate(float dt) {}

		// Called every frame between BeginFrame/EndFrame
		virtual void OnRender() {}

		// Called before shutdown
		virtual void OnShutdown() {}
	};

	// Run the main engine loop with a given game app.
	// Blocks until the window closes or OnUpdate triggers a quit.
	void Run(GameApp& app, const char* title = "MiniEngine Game", int width = 1280, int height = 720, bool vsync = true, int targetFps = 60);

	// Requests the engine loop to exit (safe to call from OnUpdate)
	void RequestQuit();

	// Returns true if a quit request has been issued.
	bool ShouldQuit();
}
