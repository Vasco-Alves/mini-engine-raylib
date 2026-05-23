#pragma once

#include <string>

namespace me {

	struct AppConfig {
		std::string title = "Mini Engine Raylib";
		int width = 1280;
		int height = 720;
		bool vsync = false;
		int target_fps = 0;
	};

	class Application {
	public:
		virtual ~Application() = default;

		// Called once when the engine starts
		virtual void on_start() {}

		// Called when the window changes size
		virtual void on_resize(int width, int height) {}

		// Called every frame. dt = delta time in seconds.
		virtual void on_update(float dt) {}

		// Called every frame after clearing the screen
		virtual void on_render() {}

		// Called when the window is closing
		virtual void on_shutdown() {}
	};

} // namespace me