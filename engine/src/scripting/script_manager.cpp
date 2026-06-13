#include "mini-engine-raylib/scripting/script_manager.hpp"
#include <iostream>
#include <optional>
#include <cstdio>
#include "mini-engine-raylib/core/engine.hpp"
#include "mini-engine-raylib/core/logger.hpp"
#include "mini-engine-raylib/core/events.hpp"
#include "mini-engine-raylib/core/time.hpp"
#include "mini-engine-raylib/ecs/components.hpp"
#include "mini-engine-raylib/ecs/audio_components.hpp"
#include "mini-engine-raylib/ecs/component_registry.hpp"
#include "mini-engine-raylib/scene/scene_manager.hpp"
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
		// Deliberately sandboxed: no io/os/debug. `table` is essential for any
		// non-trivial gameplay code (inventories, lists of spawned entities, ...).
		s_State->open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);

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
			sol::call_constructor, sol::constructors<Vector3(), Vector3(float, float, float)>(),
			"x", &Vector3::x,
			"y", &Vector3::y,
			"z", &Vector3::z,

			sol::meta_function::addition, [](const Vector3& a, const Vector3& b) { return Vector3Add(a, b); },
			sol::meta_function::subtraction, [](const Vector3& a, const Vector3& b) { return Vector3Subtract(a, b); },
			sol::meta_function::multiplication, sol::overload(
				[](const Vector3& v, float s) { return Vector3Scale(v, s); },
				[](float s, const Vector3& v) { return Vector3Scale(v, s); }),
			sol::meta_function::division, [](const Vector3& v, float s) { return Vector3Scale(v, 1.0f / s); },
			sol::meta_function::unary_minus, [](const Vector3& v) { return Vector3Negate(v); },
			sol::meta_function::equal_to, [](const Vector3& a, const Vector3& b) { return Vector3Equals(a, b) != 0; },
			sol::meta_function::to_string, [](const Vector3& v) {
				char buf[64];
				std::snprintf(buf, sizeof(buf), "(%.3f, %.3f, %.3f)", v.x, v.y, v.z);
				return std::string(buf);
			},

			"length", [](const Vector3& v) { return Vector3Length(v); },
			"length_sq", [](const Vector3& v) { return Vector3LengthSqr(v); },
			"normalized", [](const Vector3& v) { return Vector3Normalize(v); },
			"dot", [](const Vector3& a, const Vector3& b) { return Vector3DotProduct(a, b); },
			"cross", [](const Vector3& a, const Vector3& b) { return Vector3CrossProduct(a, b); },
			"distance", [](const Vector3& a, const Vector3& b) { return Vector3Distance(a, b); },
			"lerp", [](const Vector3& a, const Vector3& b, float t) { return Vector3Lerp(a, b, t); }
		);

		s_State->new_usertype<me::Color>("Color",
			sol::constructors<me::Color(), me::Color(std::uint8_t, std::uint8_t, std::uint8_t), me::Color(std::uint8_t, std::uint8_t, std::uint8_t, std::uint8_t)>(),
			sol::call_constructor, sol::constructors<me::Color(), me::Color(std::uint8_t, std::uint8_t, std::uint8_t), me::Color(std::uint8_t, std::uint8_t, std::uint8_t, std::uint8_t)>(),
			"r", &me::Color::r,
			"g", &me::Color::g,
			"b", &me::Color::b,
			"a", &me::Color::a
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

		s_State->new_usertype<me::components::MaterialComponent>("MaterialComponent",
			sol::no_constructor,
			"albedo", &me::components::MaterialComponent::albedo,
			"roughness", &me::components::MaterialComponent::roughness,
			"metallic", &me::components::MaterialComponent::metallic,
			"emission_power", &me::components::MaterialComponent::emission_power,
			"transmission", &me::components::MaterialComponent::transmission,
			"ior", &me::components::MaterialComponent::ior,
			"tint_strength", &me::components::MaterialComponent::tint_strength
		);

		s_State->new_usertype<me::components::LightComponent>("LightComponent",
			sol::no_constructor,
			"color", &me::components::LightComponent::color,
			"intensity", &me::components::LightComponent::intensity,
			"radius", &me::components::LightComponent::radius
		);

		s_State->new_usertype<me::components::DirectionalLightComponent>("DirectionalLightComponent",
			sol::no_constructor,
			"color", &me::components::DirectionalLightComponent::color,
			"intensity", &me::components::DirectionalLightComponent::intensity,
			"angular_radius", &me::components::DirectionalLightComponent::angular_radius
		);

		s_State->new_usertype<me::components::CameraComponent>("CameraComponent",
			sol::no_constructor,
			"target", &me::components::CameraComponent::target,
			"up", &me::components::CameraComponent::up,
			"fov", &me::components::CameraComponent::fov,
			"active", &me::components::CameraComponent::active
		);

		// ===================================================================
		// 3. ENTITY BINDINGS
		// ===================================================================
		s_State->new_usertype<me::Entity>("Entity",
			sol::no_constructor,
			"is_valid", &me::Entity::is_valid,
			"destroy", &me::Entity::destroy,
			"get_id", &me::Entity::get_id,

			"get_name", [](me::Entity& e) -> std::string {
				if (!e.is_valid()) return "";
				auto* tag = e.try_get_component<me::components::TagComponent>();
				return tag ? tag->name : "";
			},

			"set_name", [](me::Entity& e, const std::string& name) {
				if (!e.is_valid()) return;
				if (auto* tag = e.try_get_component<me::components::TagComponent>()) tag->name = name;
				else e.add_component<me::components::TagComponent>({ name });
			},

			// --- component access (nil when the component is missing) ---

			"get_transform", [](me::Entity& e) -> me::components::TransformComponent* {
				// Dead entities answer nil — that's the documented contract (scripts
				// hold handles across frames and check them), not an error worth
				// logging at frame rate.
				if (!e.is_valid()) return nullptr;
				return e.try_get_component<me::components::TransformComponent>();
			},

			"get_material", [](me::Entity& e) -> me::components::MaterialComponent* {
				if (!e.is_valid()) return nullptr;
				return e.try_get_component<me::components::MaterialComponent>();
			},

			"get_light", [](me::Entity& e) -> me::components::LightComponent* {
				if (!e.is_valid()) return nullptr;
				return e.try_get_component<me::components::LightComponent>();
			},

			"get_directional_light", [](me::Entity& e) -> me::components::DirectionalLightComponent* {
				if (!e.is_valid()) return nullptr;
				return e.try_get_component<me::components::DirectionalLightComponent>();
			},

			"get_camera", [](me::Entity& e) -> me::components::CameraComponent* {
				if (!e.is_valid()) return nullptr;
				return e.try_get_component<me::components::CameraComponent>();
			},

			// --- physics (all no-ops unless the simulation is running and the
			//     entity has a RigidBody + collider) ---

			"set_velocity", [](me::Entity& ent, float x, float y, float z) {
				if (!ent.is_valid()) return;
				me::physics::set_linear_velocity(ent.get_id(), x, y, z);
			},

			"set_local_velocity", [](me::Entity& ent, float x, float y, float z) {
				if (!ent.is_valid()) return;
				me::physics::set_local_linear_velocity(ent.get_id(), x, y, z);
			},

			"get_velocity", [](me::Entity& ent) -> Vector3 {
				if (!ent.is_valid()) return { 0.0f, 0.0f, 0.0f };
				return me::physics::get_linear_velocity(ent.get_id());
			},

			"apply_impulse", [](me::Entity& ent, float x, float y, float z) {
				if (!ent.is_valid()) return;
				me::physics::apply_impulse(ent.get_id(), x, y, z);
			},

			"set_angular_velocity", [](me::Entity& ent, float x, float y, float z) {
				if (!ent.is_valid()) return;
				me::physics::set_angular_velocity(ent.get_id(), x, y, z);
			},

			// Moves the transform AND the Jolt body (when one exists), so a
			// physics entity doesn't snap back on the next simulation step.
			"teleport", [](me::Entity& ent, float x, float y, float z) {
				if (!ent.is_valid()) return;
				if (auto* t = ent.try_get_component<me::components::TransformComponent>()) {
					t->position = { x, y, z };
					t->is_dirty = true;
				}
				me::physics::teleport_body(ent.get_id(), x, y, z);
			},

			"add_offset", [](me::Entity& ent, float x, float y, float z) {
				if (!ent.is_valid()) return;
				auto* t = ent.try_get_component<me::components::TransformComponent>();
				if (t) {
					t->position.x += x;
					t->position.y += y;
					t->position.z += z;
				}
			},

			// --- audio (consumed by the audio system the same frame) ---

			"play_sound", [](me::Entity& ent) {
				if (!ent.is_valid()) return;
				auto* audio = ent.try_get_component<me::components::AudioSourceComponent>();
				if (audio) {
					audio->trigger_play = true;
				}
			},

			"play_music", [](me::Entity& ent) {
				if (!ent.is_valid()) return;
				if (auto* m = ent.try_get_component<me::components::BackgroundMusicComponent>()) m->trigger_play = true;
			},

			"stop_music", [](me::Entity& ent) {
				if (!ent.is_valid()) return;
				if (auto* m = ent.try_get_component<me::components::BackgroundMusicComponent>()) m->trigger_stop = true;
			},

			"pause_music", [](me::Entity& ent) {
				if (!ent.is_valid()) return;
				if (auto* m = ent.try_get_component<me::components::BackgroundMusicComponent>()) m->trigger_pause = true;
			},

			"resume_music", [](me::Entity& ent) {
				if (!ent.is_valid()) return;
				if (auto* m = ent.try_get_component<me::components::BackgroundMusicComponent>()) m->trigger_resume = true;
			}
		);

		// ===================================================================
		// 4. SCENE BINDINGS
		// ===================================================================
		sol::table scene_table = s_State->create_named_table("Scene");

		// First alive entity whose Tag matches, or nil.
		scene_table.set_function("find", [](const std::string& name) -> std::optional<me::Entity> {
			auto& reg = me::get_registry();
			auto& tags = reg.view<me::components::TagComponent>();
			for (size_t i = 0; i < tags.size(); ++i) {
				if (tags.components[i].name == name && reg.is_alive(tags.entity_map[i]))
					return me::Entity{ tags.entity_map[i], &reg };
			}
			return std::nullopt;
		});

		// Every alive entity whose Tag matches, as an array-style table.
		scene_table.set_function("find_all", [](const std::string& name) {
			auto& reg = me::get_registry();
			sol::table result = s_State->create_table();
			int n = 1;
			auto& tags = reg.view<me::components::TagComponent>();
			for (size_t i = 0; i < tags.size(); ++i) {
				if (tags.components[i].name == name && reg.is_alive(tags.entity_map[i]))
					result[n++] = me::Entity{ tags.entity_map[i], &reg };
			}
			return result;
		});

		// A fresh empty entity (Tag + Transform), like the editor's Create Entity.
		scene_table.set_function("create", [](const std::string& name) -> me::Entity {
			auto& reg = me::get_registry();
			auto e = reg.create_entity();
			reg.add_component<me::components::TagComponent>(e.get_id(), { name });
			reg.add_component<me::components::TransformComponent>(e.get_id(), {});
			return me::Entity{ e.get_id(), &reg };
		});

		// Clones an existing entity — the poor man's prefab: keep a template
		// entity in the scene and spawn copies of it. The clone joins the
		// source's parent but never adopts its children (same rule as the
		// editor's Duplicate), and becomes physical immediately when the
		// simulation is running.
		scene_table.set_function("spawn", [](me::Entity& src) -> std::optional<me::Entity> {
			if (!src.is_valid()) {
				me::logger::error("Scene.spawn: the source entity is invalid");
				return std::nullopt;
			}
			auto& reg = me::get_registry();
			auto clone = reg.create_entity();
			me::ecs::clone_entity(reg, src.get_id(), clone.get_id());

			if (auto* t = reg.try_get_component<me::components::TransformComponent>(clone.get_id())) {
				t->children.clear();
				if (t->parent != me::entity::null) {
					if (auto* p = reg.try_get_component<me::components::TransformComponent>(t->parent))
						p->add_child(clone.get_id());
					else
						t->parent = me::entity::null;
				}
				t->is_dirty = true;
			}

			me::physics::add_body(reg, clone.get_id());
			return me::Entity{ clone.get_id(), &reg };
		});

		// Deferred scene switch: applied by the engine at the end of the frame.
		scene_table.set_function("load", [](const std::string& path) {
			me::scene_manager::request_load(path);
		});

		// ===================================================================
		// 5. PHYSICS BINDINGS
		// ===================================================================
		sol::table physics_table = s_State->create_named_table("Physics");

		// Closest hit as a table {entity, point, normal, distance}, or nil.
		// Only sees physics bodies (RigidBody + collider), and only in play mode.
		physics_table.set_function("raycast",
			[](const Vector3& origin, const Vector3& direction, sol::optional<float> max_distance) -> sol::object {
				me::physics::RaycastHit hit = me::physics::raycast(origin, direction, max_distance.value_or(1000.0f));
				if (!hit.hit) return sol::make_object(*s_State, sol::lua_nil);

				sol::table t = s_State->create_table();
				t["entity"] = me::Entity{ hit.entity, &me::get_registry() };
				t["point"] = hit.point;
				t["normal"] = hit.normal;
				t["distance"] = hit.distance;
				return sol::make_object(*s_State, t);
			});

		// ===================================================================
		// 6. INPUT BINDINGS
		// ===================================================================
		sol::table input_table = s_State->create_named_table("Input");

		input_table.set_function("action_down", &me::input::action_down);
		input_table.set_function("action_pressed", &me::input::action_pressed);
		input_table.set_function("action_released", &me::input::action_released);
		input_table.set_function("axis_value", &me::input::axis_value);

		// ===================================================================
		// 7. ENGINE BINDINGS
		// ===================================================================
		sol::table engine_table = s_State->create_named_table("Engine");

		// Ends the game session. What that means is the front-end's call:
		// the game runtime closes, the editor stops play mode.
		engine_table.set_function("quit", []() {
			me::get_event_bus().publish<me::events::QuitRequestedEvent>();
		});

		// Seconds since the application started.
		engine_table.set_function("time", []() {
			return me::time::elapsed();
		});

		engine_table.set_function("warn", [](const std::string& msg) { me::logger::warn("[LUA] " + msg); });
		engine_table.set_function("error", [](const std::string& msg) { me::logger::error("[LUA] " + msg); });
	}

	sol::state& get_state() {
		if (!s_State) {
			me::logger::error("scripting::get_state() called before init() — aborting");
			std::abort();
		}
		return *s_State;
	}

} // namespace me::scripting