#include "sandbox/core/game.hpp"

#include <mini-engine-raylib/input/input.hpp>
#include <mini-engine-raylib/core/engine.hpp>
#include <mini-engine-raylib/scene/scene_manager.hpp>
#include <mini-engine-raylib/render/renderer.hpp>
#include <mini-engine-raylib/render/camera_system.hpp>

namespace sandbox {

	void SandboxGame::on_start(int width, int height) {
		// --- Load Scene using the Raylib Scene Manager
		me::scene::manager::register_scene(&m_test_scene);
		me::scene::manager::load("Level3D");
	}

	void SandboxGame::on_update(float dt) {
		if (me::input::action_pressed("Quit")) {
			me::close_application();
		}

		me::scene::manager::update(dt);
		me::camera::update_free_fly(dt);
	}

	void SandboxGame::on_render() {
		// Draw the 3D world
		me::render::clear_world(me::Color{ 20, 20, 20, 255 });
		me::render::render_world();
	}

}