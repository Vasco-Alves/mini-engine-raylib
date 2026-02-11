#include "GameApp.hpp"
#include "SceneManager.hpp"
#include "GameScene.hpp"
#include "Components.hpp"
#include "Entity.hpp"
#include "Input.hpp"
#include "Render3D.hpp"
#include "CameraSystem.hpp"
#include "Color.hpp"

#include <raylib.h> // For DisableCursor/EnableCursor

class Level3D : public me::scene::GameScene {
public:
	void OnRegister() override {
		// Up/Down (Y-axis) - Fly controls
		me::input::BindDigitalAxis("MoveY", me::input::Key::Q, me::input::Key::E);
	}

	void OnEnter() override {
		// Lock the cursor to the window so we can look around infinitely
		DisableCursor();

		// 1. Create Main Camera (The Player)
		auto cam = me::Entity::Create("MainCamera");
		cam.Add(me::components::Transform{ 0.0f, 5.0f, 10.0f });
		cam.Add(me::components::Camera{});
		me::camera::LookAt(cam.Id(), 0.0f, 0.0f, 0.0f);

		// 2. Create Floor
		auto floor = me::Entity::Create("Floor");
		floor.Add(me::components::Transform{ 0, 0, 0, 0,0,0, 20, 1, 20 }); // Scale 20x
		floor.Add(me::components::MeshRenderer{ me::components::MeshRenderer::Plane, me::Color::DarkGray });

		// 3. Create Reference Cube
		auto cube = me::Entity::Create("RefCube");
		cube.Add(me::components::Transform{ 0, 2, 0 });
		cube.Add(me::components::MeshRenderer{ me::components::MeshRenderer::Cube, me::Color::Red });

	}

	void OnExit() override {
		// Release cursor when leaving the level
		EnableCursor();
	}

	const char* GetName() const override { return "Level3D"; }
};

class SandboxApp : public me::GameApp {
public:
	void OnStart() override {
		me::input::BindAction("Quit", me::input::Key::Escape);

		// Register and Load
		me::scene::manager::Register(&level1);
		me::scene::manager::Load("Level3D");
	}

	void OnUpdate(float dt) override {
		if (me::input::ActionPressed("Quit")) me::RequestQuit();

		// 1. Update Game Logic
		me::scene::manager::Update(dt);

		// 2. Update Camera (Free Fly)
		// This reads LookX/Y and MoveX/Y/Z to move the active camera
		me::camera::UpdateFreeFly(dt);
	}

	void OnRender() override {
		me::render::ClearWorld(me::Color{ 20, 20, 20, 255 });
		me::render::RenderWorld();
	}

private:
	Level3D level1;
};

int main() {
	SandboxApp app;
	me::Run(app, "MiniEngine 3D - FPS Camera", 1280, 720, true);
}