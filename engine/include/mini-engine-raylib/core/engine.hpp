#pragma once

#include <string>

#include "mini-engine-raylib/core/application.hpp"

namespace me {

	class Registry;

	bool init(const AppConfig& config);
	void run(Application& app, const AppConfig& config = {});

	// Advances the simulation one frame (scripts -> physics -> transforms), honoring
	// play/pause/step state. run() already calls this each frame before on_update();
	// exposed for front-ends that drive their own loop.
	void world_update(float dt);

	// Global Accessors
	Registry& get_registry();

	// --- GAME STATE ---
	void set_playing(bool playing);
	bool is_playing();

	void close_application();

	int get_window_width();
	int get_window_height();

	// --- Time Controls ---
	void set_paused(bool paused);
	bool is_paused();
	void step(int frames = 1);

}