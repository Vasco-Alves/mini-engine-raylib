#pragma once

#include "mini-engine-raylib/scene/scene.hpp"

#include <string>

namespace me {

	struct AppConfig {
		std::string title = "Mini Engine Game";
		int width = 1280;
		int height = 720;
		bool vsync = false;
		int target_fps = 0;
	};

	class Application {
	public:
		virtual ~Application() = default;

		// Called once when the engine starts
		virtual void on_start(int width, int height) {}

		// Called when the window changes size
		virtual void on_resize(int width, int height) {
			if (m_active_scene)
				m_active_scene->on_resize(width, height);
		}

		// Called every frame. dt = delta time in seconds.
		virtual void on_update(float dt) {}

		// Called every frame after clearing the screen
		virtual void on_render() {
			if (m_active_scene)
				m_active_scene->on_render();
		}

		// Called when the window is closing
		virtual void on_shutdown() {
			if (m_active_scene)
				m_active_scene->on_close();
		}

		// Loads new scene into memory
		void load_scene(std::shared_ptr<Scene> scene, int current_width, int current_height) {
			if (m_active_scene)
				m_active_scene->on_close();

			m_active_scene = scene;
			m_active_scene->on_start(current_width, current_height);
		}

	protected:
		std::shared_ptr<Scene> m_active_scene = nullptr;
	};

} // namespace me