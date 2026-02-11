#pragma once

namespace me {

	struct EngineConfig {
		const char* title = "MiniEngine";
		int width = 1280;
		int height = 720;
		bool vsync = false;
		int targetFps = 0;
	};

	bool Init(const EngineConfig& cfg = {});

	bool Update(); // returns false if the app should exit

	void BeginFrame(); // starts rendering

	void EndFrame(); // ends rendering and presents

	void Shutdown();

	// Optional convenience for prototypes
	inline bool Tick() {
		if (!Update())
			return false;

		BeginFrame();
		EndFrame();

		return true;
	}

} // namespace me
