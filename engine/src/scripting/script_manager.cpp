#include "mini-engine-raylib/scripting/script_manager.hpp"

#include <iostream>

#include "mini-engine-raylib/core/engine.hpp"
#include "mini-engine-raylib/ecs/components.hpp"

#include <mini-ecs/registry.hpp>
#include <mini-ecs/entity.hpp>     

namespace me::scripting {

	namespace {
		// Global Lua brain. Hidden from the rest of the engine.
		std::unique_ptr<sol::state> s_State = nullptr;
	}

	void init() {
		if (s_State)
			return;

		s_State = std::make_unique<sol::state>();

		// Open standard Lua libraries
		s_State->open_libraries(sol::lib::base, sol::lib::math, sol::lib::string);

		// Automatically teach Lua about our C++ engine
		bind_engine();
	}

	void shutdown() {
		s_State.reset();
	}

	void bind_engine() {
		if (!s_State) return;

		// ===================================================================
		// 1. MATH BINDINGS
		// ===================================================================
		s_State->new_usertype<Vector3>("Vector3",
			sol::constructors<Vector3(), Vector3(float, float, float)>(),
			"x", &Vector3::x,
			"y", &Vector3::y,
			"z", &Vector3::z
		);

		// ===================================================================
		// 2. COMPONENT BINDINGS
		// ===================================================================
		s_State->new_usertype<me::components::TransformComponent>("TransformComponent",
			// Allow Lua to create new Transforms
			sol::constructors<me::components::TransformComponent()>(),

			// Map the C++ Vector3 structs to Lua!
			"position", &me::components::TransformComponent::position,
			"rotation", &me::components::TransformComponent::rotation,
			"scale", &me::components::TransformComponent::scale
		);

		// ===================================================================
		// 3. ENTITY BINDINGS (The Object-Oriented Upgrade!)
		// ===================================================================
		s_State->new_usertype<me::Entity>("Entity",
			sol::no_constructor, // Lua scripts receive entities from the Engine, they don't spawn raw ones.

			// Let Lua check if the entity is alive
			"is_valid", &me::Entity::is_valid,

			// Let Lua destroy the entity
			"destroy", &me::Entity::destroy,

			// Let Lua grab the transform using the clean wrapper method
			"get_transform", [](me::Entity& e) -> me::components::TransformComponent* {
				return e.try_get_component<me::components::TransformComponent>();
			}
		);
	}

	sol::state& get_state() {
		return *s_State;
	}

} // namespace me::scripting