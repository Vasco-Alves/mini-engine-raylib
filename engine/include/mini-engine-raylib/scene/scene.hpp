#pragma once

#include "mini-engine-raylib/ecs/system.hpp"

#include <mini-ecs/registry.hpp>

#include <vector>
#include <memory>

namespace me {

	class Scene {
	public:
		virtual ~Scene() = default;

		virtual void on_start(int width, int height) {}

		virtual void on_resize(int width, int height) {}

		virtual void on_update(float dt) {
			for (auto& system : systems)
				system->on_update(dt);
		}

		virtual void on_render() {
			for (auto& system : systems)
				system->on_render();
		}

		virtual void on_close() {}

	protected:
		template<typename T, typename... Args>
		void add_system(Args&&... args) {
			systems.push_back(std::make_unique<T>(m_registry, std::forward<Args>(args)...));
		}

	protected:
		me::Registry m_registry;
		std::vector<std::unique_ptr<System>> systems;
	};

	namespace scene {
		bool save(const char* filename);
		bool load(const char* filename);
	}

} // namespace me
