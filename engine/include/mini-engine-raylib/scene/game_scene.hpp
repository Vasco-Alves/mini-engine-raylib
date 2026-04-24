#pragma once

#include "mini-engine-raylib/scene/scene.hpp"
#include "mini-engine-raylib/render/color.hpp"

namespace me::scene {

	class GameScene {
	public:
		virtual ~GameScene() = default;

		// convenience: save this scene's world back to its file
		bool save_self() const {
			const char* file = get_file();
			return (file && *file) ? me::scene::save(file) : false; // Uses me::scene::save() from scene.cpp
		}

		virtual const char* get_name() const = 0;

		// JSON file for this scene, or "" / nullptr for code-only scenes
		virtual const char* get_file() const {
			return "";
		}

		virtual me::Color get_clear_color() const {
			return me::Color{ 20, 20, 20, 255 };
		}

		// Optional hooks
		virtual void on_register() {}
		virtual void on_enter() {}
		virtual void on_exit() {}
		virtual void on_update(float /*dt*/) {}
	};

} // namespace me::scene