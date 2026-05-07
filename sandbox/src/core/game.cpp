#include "sandbox/core/game.hpp"

#include <mini-engine-raylib/input/input.hpp>
#include <mini-engine-raylib/core/engine.hpp>
#include <mini-engine-raylib/scene/scene.hpp>
#include <mini-engine-raylib/render/renderer.hpp>
#include <mini-engine-raylib/render/camera_system.hpp>
#include <mini-engine-raylib/systems/script_system.hpp>
#include <mini-engine-raylib/scripting/script_manager.hpp>

namespace sandbox {

	void SandboxGame::on_start() {
		me::input::bind_action("Quit", me::input::Key::Escape);

		me::scene_manager::register_scene(&m_test_scene);
		me::scene_manager::load("Level3D");
	}

	void SandboxGame::on_resize(int width, int height) {
		me::scene_manager::resize(width, height);
	}

	void SandboxGame::on_update(float dt) {
		if (me::input::action_pressed("Quit"))
			me::close_application();

		me::scene_manager::update(dt);
		me::camera::update_free_fly(dt);
		me::systems::script_update(dt);
	}

	void SandboxGame::on_render() {
		me::Color c = me::scene_manager::current()->get_clear_color();
		me::render::clear_world(c);
		me::render::render_world();
	}

	void SandboxGame::on_shutdown() {
		me::scene_manager::exit();
	}

}