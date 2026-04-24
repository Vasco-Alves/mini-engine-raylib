#pragma once

#include <mini-engine-raylib/scene/game_scene.hpp>
#include <mini-engine-raylib/input/input.hpp>
#include <mini-engine-raylib/core/engine.hpp>
#include <mini-engine-raylib/ecs/components.hpp>
#include <mini-engine-raylib/render/camera_system.hpp>

namespace sandbox {

	class TestScene : public me::scene::GameScene {
	public:
		void on_register() override {
			me::input::bind_digital_axis("MoveY", me::input::Key::Q, me::input::Key::E);
		}

		void on_enter() override {
			me::input::lock_cursor();
			auto& reg = me::get_registry();

			// 1. Create Main CameraComponent (The Player)
			auto cam = reg.create_entity("MainCamera");
			reg.add_component(cam, me::components::TransformComponent{ 0.0f, 5.0f, 10.0f });
			reg.add_component(cam, me::components::CameraComponent{});
			me::camera::look_at(cam, 0.0f, 0.0f, 0.0f);

			// 2. Create Floor
			auto floor = reg.create_entity("Floor");
			reg.add_component(floor, me::components::TransformComponent{ 0, 0, 0, 0,0,0, 20, 1, 20 });
			reg.add_component(floor, me::components::MeshRendererComponent{ me::components::MeshRendererComponent::Plane, me::Color::dark_gray });

			// 3. Create Reference Cube
			auto cube = reg.create_entity("RefCube");
			reg.add_component(cube, me::components::TransformComponent{ 0, 2, 0 });
			reg.add_component(cube, me::components::MeshRendererComponent{ me::components::MeshRendererComponent::Sphere, me::Color::red });
		}

		void on_exit() override {
			me::input::unlock_cursor();
		}

		const char* get_name() const override {
			return "Level3D";
		}
	};

} // namespace sandbox