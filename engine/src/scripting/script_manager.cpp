#include "mini-engine-raylib/scripting/script_manager.hpp"
#include <iostream>
#include "mini-engine-raylib/core/engine.hpp"
#include "mini-engine-raylib/core/logger.hpp"
#include "mini-engine-raylib/ecs/components.hpp"
#include "mini-engine-raylib/systems/physics_system.hpp"
#include "mini-engine-raylib/input/input.hpp"
#include <mini-ecs/registry.hpp>
#include <mini-ecs/entity.hpp>     

namespace me::scripting {

	namespace {
		// Global Lua brain
		std::unique_ptr<sol::state> s_State = nullptr;
	}

	void init() {
		if (s_State)
			return;

		s_State = std::make_unique<sol::state>();
		s_State->open_libraries(sol::lib::base, sol::lib::math, sol::lib::string);

		// Custom print function to route Lua print() to our Engine Logger!
		s_State->set_function("print", [](sol::variadic_args va) {
			std::string output = "";
			for (auto v : va) {
				try {
					output += v.as<std::string>() + " ";
				} catch (...) {
					output += "[Object] ";
				}
			}

			// Broadcast the message to whoever is listening
			me::logger::info("[LUA] " + output);
			});

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

			// Map the C++ Vector3 structs to Lua
			"position", &me::components::TransformComponent::position,
			"rotation", &me::components::TransformComponent::rotation,
			"scale", &me::components::TransformComponent::scale
		);

		// ===================================================================
		// 3. ENTITY BINDINGS
		// ===================================================================
		s_State->new_usertype<me::Entity>("Entity",
			sol::no_constructor, // Lua scripts receive entities from the Engine, they don't spawn raw ones.
			"is_valid", &me::Entity::is_valid,
			"destroy", &me::Entity::destroy,

			"get_transform", [](me::Entity& e) -> me::components::TransformComponent* {
				return e.try_get_component<me::components::TransformComponent>();
			},

			// The Physics Velocity hook!
			"set_velocity", [](me::Entity& ent, float x, float y, float z) {
				me::physics::set_linear_velocity(ent.get_id(), x, y, z);
			}
		);

		// ===================================================================
		// 4. INPUT BINDINGS
		// ===================================================================
		sol::table input_table = s_State->create_named_table("Input");

		input_table.set_function("action_down", &me::input::action_down);
		input_table.set_function("action_pressed", &me::input::action_pressed);
		input_table.set_function("action_released", &me::input::action_released);
		input_table.set_function("axis_value", &me::input::axis_value);
	}

	sol::state& get_state() {
		return *s_State;
	}

} // namespace me::scripting