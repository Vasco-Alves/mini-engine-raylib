#pragma once

#include <string>

#include "mini-engine-raylib/core/application.hpp"

namespace me {

	class Registry;

	bool init(const AppConfig& config);
	void run(Application& app, const AppConfig& config = {});

	// Global Accessors
	Registry& get_registry();

	// --- GAME STATE ---
	void set_playing(bool playing);
	bool is_playing();

	void close_application();

	int get_window_width();
	int get_window_height();

}