#pragma once

#include "mini-engine-raylib/core/engine.hpp"
#include "mini-engine-raylib/render/color.hpp"
#include "mini-engine-raylib/ecs/system.hpp"

#include <string>
#include <vector>
#include <memory>

namespace me {

	// ===================================================================
	// 1. THE BASE SCENE CLASS (Inherit from this to create your levels)
	// ===================================================================
	class Scene {
	public:
		virtual ~Scene() = default;

		virtual const char* get_name() const = 0;

		// Optional JSON file for this scene (returns "" if code-only)
		virtual const char* get_file() const { return ""; }
		//virtual me::Color get_clear_color() const { return me::Color{ 20, 20, 20, 255 }; }

		template <typename T, typename... Args>
		void add_system(Args&&... args) {
			m_systems.push_back(std::make_unique<T>(std::forward<Args>(args)...));
		}

		// Lifecycle hooks
		virtual void on_register() {}
		virtual void on_enter() {}

		virtual void on_exit() {
			m_systems.clear();
		}

		virtual void on_update(float dt) {
			auto& reg = me::get_registry();

			for (auto& sys : m_systems)
				sys->on_update(reg, dt);
		}

		virtual void on_resize(int width, int height) {}

		// Saves/Loads this scene's entities to/from its JSON file
		bool save_to_file() const;
		bool load_from_file() const;

	protected:
		std::vector<std::unique_ptr<System>> m_systems;
	};

	// ===================================================================
	// 2. THE SCENE MANAGER (Handles switching between levels)
	// ===================================================================
	namespace scene_manager {
		void register_scene(Scene* scene);        // Registers a level
		void load(const std::string& name);       // Switches the active level
		void exit();
		void update(float dt);                    // Updates the active level
		void resize(int width, int height);

		const std::string& current_name();
		Scene* current();
	}

} // namespace me