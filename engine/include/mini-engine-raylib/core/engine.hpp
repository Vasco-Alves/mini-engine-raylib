#pragma once

#include "mini-engine-raylib/core/application.hpp"

#include <string>

namespace me {

	class Registry;

	// The main entry point. 
	// Initializes Modulus, creates the Window, runs the Loop, and cleans up.
	// Blocks until the game is closed.
	void run(Application& app, const AppConfig& config = {});

	// Global Accessors
	Registry& get_registry();

	// Request the engine to stop (e.g. from a Quit button)
	void close_application();

}