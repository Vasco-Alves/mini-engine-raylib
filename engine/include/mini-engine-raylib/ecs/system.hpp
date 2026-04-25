#pragma once

#include <mini-ecs/registry.hpp>

namespace me {

	class System {
	public:
		virtual ~System() = default;

		virtual void on_update(Registry& registry, float dt) = 0;
	};

} // namespace me