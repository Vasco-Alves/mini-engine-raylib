#pragma once

#include "mini-engine-raylib/core/application.hpp"

#include <string>

namespace me {

	class Registry;

	bool init(const AppConfig& config);
	void run(Application& app, const AppConfig& config = {});

	// Global Accessors
	Registry& get_registry();

	// Request the engine to stop (e.g. from a Quit button)
	void close_application();

	int get_window_width();
	int get_window_height();

}