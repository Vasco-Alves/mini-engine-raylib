#include "sandbox/scene/testscene.hpp"

#include <mini-engine-raylib/input/input.hpp>
#include <mini-engine-raylib/core/engine.hpp>
#include <mini-engine-raylib/ecs/components.hpp>
#include <mini-engine-raylib/ecs/script_component.hpp>
#include <mini-engine-raylib/render/camera_system.hpp>

namespace sandbox {

	void TestScene::on_resize(int width, int height) {

	}

	void TestScene::on_register() {
		me::input::bind_digital_axis("MoveY", me::input::Key::Q, me::input::Key::E);
	}

	void TestScene::on_enter() {
		me::input::lock_cursor();
		auto& reg = me::get_registry();

		// 1. Create Main CameraComponent (The Player)
		auto cam = reg.create_entity("MainCamera");
		reg.add_component(cam, me::components::TransformComponent{ 0.0f, 5.0f, 10.0f });
		reg.add_component(cam, me::components::CameraComponent{});
		me::camera::look_at(cam, 0.0f, 0.0f, 0.0f);

		// 2. Create Floor
		auto floor = reg.create_entity("Floor");
		//reg.add_component(floor, me::components::TagComponent{ "Floor" });
		reg.add_component(floor, me::components::TransformComponent{ 0, 0, 0, 0,0,0, 20, 1, 20 });
		reg.add_component(floor, me::components::Shape3DComponent{ me::components::Shape3DComponent::Plane, me::Color::dark_gray });


		// 3. Create Reference Cube
		auto cube = reg.create_entity("RefCube");
		reg.add_component(cube, me::components::TransformComponent{ 0, 2, 0 });
		reg.add_component(cube, me::components::Shape3DComponent{ me::components::Shape3DComponent::Cube, me::Color::red });

		// LUA SCRIPT
		reg.add_component(cube, me::components::ScriptComponent{ "assets/scripts/rotator.lua" });
	}

	void TestScene::on_update(float dt) {
		me::Scene::on_update(dt);
	}

	void TestScene::on_exit() {
		me::input::unlock_cursor();
		me::assets::release_all();
		me::Scene::on_exit();
	}

}