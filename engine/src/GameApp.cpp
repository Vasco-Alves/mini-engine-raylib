#include "GameApp.hpp"
#include "Engine.hpp"
#include "Time.hpp"
#include "Render2D.hpp"
#include "Input.hpp"

namespace {
	bool g_ShouldQuit = false;
}

namespace me {

	void RequestQuit() {
		g_ShouldQuit = true;
	}

	bool ShouldQuit() {
		return g_ShouldQuit;
	}

	void Run(GameApp& app, const char* title, int width, int height, bool vsync, int targetFps) {
		me::EngineConfig cfg;
		cfg.title = title;
		cfg.width = width;
		cfg.height = height;
		cfg.vsync = vsync;
		cfg.targetFps = targetFps;

		if (!me::Init(cfg))
			return;

		app.OnStart();

		while (!ShouldQuit() && me::Update()) {
			float dt = me::time::Delta();

			app.OnUpdate(dt);

			me::BeginFrame();
			app.OnRender();
			me::EndFrame();
		}

		app.OnShutdown();
		me::Shutdown();
	}

} // namespace me
