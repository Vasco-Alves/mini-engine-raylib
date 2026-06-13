#include "mini-engine-raylib/systems/script_system.hpp"

#include <iostream>
#include <filesystem>
#include <unordered_map>

#include "mini-engine-raylib/core/engine.hpp"
#include "mini-engine-raylib/ecs/script_component.hpp"
#include "mini-engine-raylib/scripting/script_manager.hpp"
#include "mini-engine-raylib/systems/physics_system.hpp"
#include "mini-engine-raylib/core/vfs.hpp"
#include "mini-engine-raylib/core/logger.hpp"
#include <mini-ecs/registry.hpp>

namespace me::systems {

	static float s_FileCheckTimer = 0.0f;

	// (Re)captures the script's callable surface from its environment — shared by
	// first-time initialization and hot-reload so they can't drift apart.
	static void cache_script_functions(me::components::ScriptInstance& script) {
		script.update_fn = script.env["update"];
		script.on_collision_enter_fn = script.env["on_collision_enter"];
		script.on_collision_exit_fn = script.env["on_collision_exit"];
		script.runtime_error_logged = false; // a fresh (re)load gets a fresh report
	}

	// Logs a script's runtime error once; repeats are suppressed until the file
	// is edited (hot-reload resets the flag) so a broken update() doesn't flood
	// the console at frame rate.
	static void report_runtime_error(me::components::ScriptInstance& script, const char* where, const sol::error& err) {
		if (script.runtime_error_logged) return;
		script.runtime_error_logged = true;
		me::logger::error("Runtime Error in " + script.path + " (" + where + "):\n" + err.what()
			+ "\n(further errors from this script are suppressed until it is edited)");
	}

	void script_update(float dt) {
		auto& reg = me::get_registry();
		auto& pool = reg.view<me::components::ScriptComponent>();
		sol::state& lua = me::scripting::get_state();

		s_FileCheckTimer += dt;
		bool should_check_files = false;
		if (s_FileCheckTimer >= 0.5f) {
			should_check_files = true;
			s_FileCheckTimer = 0.0f;
		}

		// Only allocate the map if checking files this frame
		std::unordered_map<std::string, std::filesystem::file_time_type> cached_timestamps;

		for (int i = (int)pool.size() - 1; i >= 0; --i) {
			auto e = pool.entity_map[i];
			auto& script_comp = pool.components[i];

			for (auto& script : script_comp.scripts) {

				// ==========================================
				// 1. FIRST-TIME INITIALIZATION
				// ==========================================
				if (!script.started) {
					std::string physical_path = me::vfs::resolve(script.path);

					// ONLY hit the hard drive if we are initializing it for the first time
					if (!std::filesystem::exists(physical_path)) continue;

					script.env = sol::environment(lua, sol::create, lua.globals());

					try {
						lua.script_file(physical_path, script.env);

						sol::protected_function start_fn = script.env["start"];
						if (start_fn.valid()) {
							start_fn(me::Entity{ e, &reg });
						}

						cache_script_functions(script);
						script.last_modified = std::filesystem::last_write_time(physical_path);

					} catch (const sol::error& err) {
						me::logger::error("Failed to load " + script.path + ":\n" + err.what());
					}
					script.started = true;
				}

				// ==========================================
				// 2. HOT-RELOAD LOGIC
				// ==========================================
				if (script.started && should_check_files) {

					std::string physical_path = me::vfs::resolve(script.path);

					// ONLY hit the hard drive if the 0.5s timer told us to check for updates
					if (!std::filesystem::exists(physical_path)) continue;

					std::filesystem::file_time_type current_time;

					if (cached_timestamps.find(physical_path) != cached_timestamps.end()) {
						current_time = cached_timestamps[physical_path];
					} else {
						current_time = std::filesystem::last_write_time(physical_path);
						cached_timestamps[physical_path] = current_time;
					}

					if (current_time > script.last_modified) {
						me::logger::info("Hot-Reloading: " + script.path);

						try {
							lua.script_file(physical_path, script.env);
							cache_script_functions(script);
							script.last_modified = current_time;
						} catch (const sol::error& err) {
							me::logger::error("Syntax Error in " + script.path + ":\n" + err.what());
							script.last_modified = current_time;
						}
					}
				}

				// ==========================================
				// 3. THE ACTUAL UPDATE LOOP
				// ==========================================
				// Notice how this loop does ZERO string comparisons and ZERO disk reads!
				if (script.started && script.update_fn.valid()) {
					auto result = script.update_fn(me::Entity{ e, &reg }, dt);

					if (!result.valid()) {
						sol::error err = result;
						report_runtime_error(script, "update", err);
					}
				}
			}
		}
	}

	// Calls one side of a contact event: every script on `self_id` that defines
	// the matching callback gets (self, other). The other entity may already be
	// dead (a body removal reports its last contacts one step later) — its handle
	// is passed anyway and simply answers is_valid() == false.
	static void dispatch_collision_side(me::Registry& reg,
		me::entity::entity_id self_id, me::entity::entity_id other_id, bool entered) {
		if (!reg.is_alive(self_id)) return;
		auto* sc = reg.try_get_component<me::components::ScriptComponent>(self_id);
		if (!sc) return;

		me::Entity self{ self_id, &reg };
		me::Entity other{ other_id, &reg };

		for (auto& script : sc->scripts) {
			if (!script.started) continue;
			auto& fn = entered ? script.on_collision_enter_fn : script.on_collision_exit_fn;
			if (!fn.valid()) continue;

			auto result = fn(self, other);
			if (!result.valid()) {
				sol::error err = result;
				report_runtime_error(script, entered ? "on_collision_enter" : "on_collision_exit", err);
			}
		}
	}

	void script_dispatch_collisions() {
		std::vector<me::physics::ContactEvent> events = me::physics::consume_contact_events();
		if (events.empty()) return;

		auto& reg = me::get_registry();
		for (const auto& ev : events) {
			dispatch_collision_side(reg, ev.a, ev.b, ev.entered);
			dispatch_collision_side(reg, ev.b, ev.a, ev.entered);
		}
	}
}