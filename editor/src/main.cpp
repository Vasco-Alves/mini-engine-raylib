#include "editor/core/editor_app.hpp"
#include <mini-engine-raylib/core/engine.hpp>

int main() {
	me::AppConfig config;
	config.title = "Mini Engine Raylib | Editor";
	config.width = 2000;
	config.height = 1200;
	config.vsync = true;

	editor::EditorApp app;
	me::run(app, config);

	return 0;
}
