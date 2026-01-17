#include "GameApp.hpp"
#include "SceneManager.hpp"
#include "Render2D.hpp"
#include "CameraFollow.hpp"
#include "Physics2D.hpp"
#include "Animation.hpp"
#include "Input.hpp"
#include "Time.hpp"
#include "Color.hpp"
#include "DebugDraw.hpp"
#include "scenes/UniverseScene.hpp"

#include <iostream>
#include <string>

class MyGame : public me::GameApp {
	UniverseScene universe;

	bool showColliders = false;

public:
	void OnStart() override {
		using me::input::Key;
		using me::input::Axis;
		using me::input::MouseButton;

		// --- Input bindings ---

		// Quit game
		me::input::BindAction("Quit", Key::Escape);

		// Camera zoom
		me::input::BindAxis("ZoomCam", Axis::MouseWheel, 0.15f);
		me::input::SetAxisClamp("ZoomCam", -0.5f, 0.5f);

		// Brake (dampen velocity)
		me::input::BindAction("Brake", Key::Space);

		// Toggle collider debug draw
		me::input::BindAction("ToggleColliders", Key::F2);

		// Mining fire (left mouse)
		me::input::BindAction("Mine", MouseButton::Left);

		// --- Scene setup ---
		me::scene::manager::Register(&universe);
		me::scene::manager::Load("Universe");
	}

	void OnUpdate(float dt) override {
		// Quit
		if (me::input::ActionPressed("Quit"))
			me::RequestQuit();

		// Toggle collider debug draw
		if (me::input::ActionPressed("ToggleColliders"))
			showColliders = !showColliders;

		// Scene-specific logic (player, projectiles, etc.)
		me::scene::manager::Update(dt);

		// Shared systems
		me::physics2d::Step(dt);
		me::camera::Update(dt);
		me::anim::Update(dt);
	}

	void OnRender() override {
		// World (camera space)
		me::render2d::BeginCamera();
		{
			me::render2d::ClearWorld(me::Color{ 10, 10, 30, 255 });
			me::render2d::RenderWorld();

			if (showColliders)
				me::dbg::DrawAllCollidersWorld();
		}
		me::render2d::EndCamera();

		// HUD in screen space
		std::string sceneText = "Scene: " + me::scene::manager::CurrentName();
		me::dbg::Text(10.0f, 10.0f, sceneText, me::Color::White, 20);

		std::string fpsText = "FPS: " + std::to_string(me::time::GetFPS());
		me::dbg::Text(10.0f, 40.0f, fpsText, me::Color::White, 20);
	}

	void OnShutdown() override {
		// Nothing special; engine shutdown will clean up.
	}
};

int main() {
	MyGame game;
	me::Run(
		game,                              // Game app instance
		"Universe Prototype – MiniEngine", // Window title
		1880,                              // Width
		1020,                               // Height
		false,                              // VSync
		0                                  // Target FPS (0 = uncapped, vsync controls it)
	);
}
