#include "GameApp.hpp"
#include "SceneManager.hpp"
#include "GameScene.hpp"
#include "Render2D.hpp"
#include "Input.hpp"
#include "Color.hpp"

#include <iostream>

// --- A Empty Test Scene ---
class TestScene : public me::scene::GameScene {
public:
	void OnEnter() override {
		std::cout << "Engine is running!\n";

		auto cam = me::Entity::Create("Camera");
		cam.Add(me::components::Transform2D{ 0, 0 });
		cam.Add(me::components::Camera2D{ 1.0f, true });
		me::render2d::SetActiveCamera(cam.Id());
	}

	void OnUpdate(float dt) override {
		step += dt;
	}

	void OnExit() override {
		std::cout << "Leaving scene!\n";
	}

	const char* GetName() const override {
		return "TestBed";
	}

	const char* GetFile() const override {
		return "testbed.json";
	}

	me::Color GetClearColor() const override {
		uint8_t r = static_cast<uint8_t>((std::sin(step) + 1.0f) * 127.0f);
		return me::Color{ r, 0, 0, 255 };
	}

private:
	float step = 0.0f;
};

// --- The Application ---
class SandboxApp : public me::GameApp {
public:
	void OnStart() override {
		me::input::BindAction("Quit", me::input::Key::Escape);

		// Register and load our test scene
		me::scene::manager::Register(&scene);
		me::scene::manager::Load("TestBed");
	}

	void OnUpdate(float dt) override {
		if (me::input::ActionPressed("Quit"))
			me::RequestQuit();

		me::scene::manager::Update(dt);
		// Add systems here as you need them (Physics, Anim, etc.)
	}

	void OnRender() override {
		me::render2d::BeginCamera();

		me::Color clearCol = me::Color{ 20, 20, 20, 255 };

		me::scene::GameScene* currentScene = me::scene::manager::Current();
		if (currentScene)
			clearCol = currentScene->GetClearColor();

		me::render2d::ClearWorld(clearCol);

		me::render2d::RenderWorld();

		me::render2d::EndCamera();
	}

	void OnShutdown() override {}

private:
	TestScene scene;
};

int main() {
	SandboxApp app;
	me::Run(app, "MiniEngine - Fresh Start", 1280, 720);
}
