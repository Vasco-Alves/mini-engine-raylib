#include "sandbox/core/game.hpp"

#include <mini-engine-raylib/core/engine.hpp>
#include <mini-engine-raylib/core/application.hpp>

int main() {
	me::AppConfig config;
	config.title = "Mini Engine Test";
	config.width = 1920;
	config.height = 1080;
	config.vsync = false;
	config.target_fps = 0;

	sandbox::SandboxGame game;
	me::run(game, config);

	return 0;
}