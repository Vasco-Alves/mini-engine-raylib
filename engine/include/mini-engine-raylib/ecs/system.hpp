#pragma once

#include <mini-ecs/registry.hpp>

namespace me {

	class System {
	public:
		explicit System(Registry& registry) : m_registry(registry) {}
		virtual ~System() = default;

		virtual void on_update(float dt) {}
		virtual void on_render() {}

	protected:
		me::Registry& m_registry;
	};

} // namespace me
