#include "mini-engine-raylib/core/engine.hpp"
#include "mini-engine-raylib/scripting/script_manager.hpp"
#include "mini-engine-raylib/ecs/components.hpp"

#include <mini-ecs/registry.hpp>

#include <iostream>

namespace me::scripting {

	namespace {
		// Our single, global Lua brain. Hidden from the rest of the engine.
		sol::state* s_State = nullptr;
	}

	void init() {
		if (s_State) return;

		s_State = new sol::state();

		// Open standard Lua libraries (gives scripts access to math, string manipulation, etc.)
		s_State->open_libraries(sol::lib::base, sol::lib::math, sol::lib::string);

		// Automatically teach Lua about our C++ engine
		bind_engine();
	}

	void shutdown() {
		if (s_State) {
			delete s_State;
			s_State = nullptr;
		}
	}

	void bind_engine() {
		if (!s_State) return;

		// 1. Bind the Transform Component
		// new_usertype maps a C++ struct to a Lua table
		s_State->new_usertype<me::components::TransformComponent>("TransformComponent",
			// Allow Lua to create new Transforms with default or specific values
			sol::constructors<me::components::TransformComponent(), me::components::TransformComponent(float, float, float)>(),

			// Map the C++ variables to Lua variable names
			"x", &me::components::TransformComponent::x,
			"y", &me::components::TransformComponent::y,
			"z", &me::components::TransformComponent::z,
			"rotx", &me::components::TransformComponent::rot_x,
			"roty", &me::components::TransformComponent::rot_y,
			"rotz", &me::components::TransformComponent::rot_z,
			"sx", &me::components::TransformComponent::sx,
			"sy", &me::components::TransformComponent::sy,
			"sz", &me::components::TransformComponent::sz
		);

		// 2. Bind the Registry
		// We use sol::no_constructor because Lua should never create a Registry itself,
		// it should only interact with the global one provided by the engine.
		s_State->new_usertype<me::Registry>("Registry",
			sol::no_constructor,
			"get_transform", [](me::Registry& reg, uint32_t entityId) -> me::components::TransformComponent* {
				return reg.try_get_component<me::components::TransformComponent>(entityId);
			}
		);

		// 3. Bind a global function to fetch the registry
		s_State->set_function("get_registry", []() -> me::Registry& {
			return me::get_registry();
			});

		// TODO: bind me::math::Vec2, me::Color, etc.
	}

	sol::state& get_state() {
		return *s_State;
	}

} // namespace me::scripting
