// Headless tests for the gameplay Lua API — no window, GPU, audio device or
// global engine state.
//
// Covers what can run cold: the maths usertypes (Vector3 operators/methods,
// Color), the sandboxed library set, and the Entity bindings against a local
// Registry (component access, name get/set, teleport's transform half, audio
// triggers, and the physics calls degrading to safe no-ops while no simulation
// is running). Scene.* and Engine.quit need the running engine loop and are
// exercised through the editor/game instead.

#include <mini-engine-raylib/scripting/script_manager.hpp>
#include <mini-engine-raylib/ecs/components.hpp>
#include <mini-engine-raylib/ecs/audio_components.hpp>
#include <mini-ecs/registry.hpp>

#include <cmath>
#include <iostream>

using namespace me::components;

static int g_failures = 0;

#define CHECK(cond)                                                            \
	do {                                                                       \
		if (!(cond)) {                                                         \
			std::cerr << "  FAIL: " << #cond << "  (line " << __LINE__ << ")\n"; \
			++g_failures;                                                      \
		}                                                                      \
	} while (0)

// Runs a Lua chunk; any error (including a failed assert()) is a test failure.
static bool run(sol::state& lua, const std::string& code) {
	sol::protected_function_result r = lua.safe_script(code, sol::script_pass_on_error);
	if (!r.valid()) {
		sol::error err = r;
		std::cerr << "  LUA ERROR: " << err.what() << "\n";
	}
	return r.valid();
}

int main() {
	std::cout << "[lua_api] running...\n";

	me::scripting::init();
	sol::state& lua = me::scripting::get_state();

	// --- sandbox: table is in, io/os stay out ---
	CHECK(run(lua, "assert(type(table.insert) == 'function')"));
	CHECK(run(lua, "assert(io == nil and os == nil)"));

	// --- Vector3 maths ---
	CHECK(run(lua, R"(
		local a = Vector3(1, 2, 3)         -- call-constructor syntax
		local b = Vector3.new(3, 2, 1)     -- classic syntax still works
		local sum = a + b
		assert(sum.x == 4 and sum.y == 4 and sum.z == 4)

		local diff = a - b
		assert(diff.x == -2 and diff.z == 2)

		local scaled = a * 2
		assert(scaled.x == 2 and scaled.y == 4 and scaled.z == 6)
		local scaled2 = 2 * a
		assert(scaled2.x == scaled.x and scaled2.y == scaled.y and scaled2.z == scaled.z)

		local halved = a / 2
		assert(math.abs(halved.y - 1) < 1e-6)

		local neg = -a
		assert(neg.x == -1 and neg.y == -2 and neg.z == -3)

		assert(Vector3(1, 2, 3) == Vector3(1, 2, 3))
		assert(not (Vector3(1, 2, 3) == Vector3(0, 2, 3)))

		assert(math.abs(Vector3(3, 4, 0):length() - 5) < 1e-6)
		assert(math.abs(Vector3(3, 4, 0):length_sq() - 25) < 1e-6)
		assert(math.abs(Vector3(10, 0, 0):normalized().x - 1) < 1e-6)
		assert(math.abs(Vector3(1, 0, 0):dot(Vector3(0, 1, 0))) < 1e-6)

		local up = Vector3(1, 0, 0):cross(Vector3(0, 1, 0))
		assert(math.abs(up.z - 1) < 1e-6)

		assert(math.abs(Vector3(0, 0, 0):distance(Vector3(0, 3, 4)) - 5) < 1e-6)

		local mid = Vector3(0, 0, 0):lerp(Vector3(10, 0, 0), 0.5)
		assert(math.abs(mid.x - 5) < 1e-6)

		assert(tostring(Vector3(1, 2, 3)):find("1.000") ~= nil)
	)"));

	// --- Color ---
	CHECK(run(lua, R"(
		local c = Color(255, 128, 0)
		assert(c.r == 255 and c.g == 128 and c.b == 0 and c.a == 255)
		c.g = 64
		assert(c.g == 64)
		local t = Color(1, 2, 3, 4)
		assert(t.a == 4)
	)"));

	// --- Entity bindings against a local registry ---
	me::Registry reg;

	auto e = reg.create_entity();
	e.add_component(TagComponent{ "Player" });
	e.add_component(TransformComponent{ { 1.0f, 2.0f, 3.0f }, { 0, 0, 0 }, { 1, 1, 1 } });
	e.add_component(MaterialComponent{ me::Color{ 200, 100, 50, 255 }, 0.5f, 0.0f, 0.0f });
	e.add_component(LightComponent{ me::Color::white, 1.0f, 0.1f });
	e.add_component(CameraComponent{});
	e.add_component(AudioSourceComponent{});      // empty filepath -> no device load
	e.add_component(BackgroundMusicComponent{});  // empty filepath -> no device load

	auto bare = reg.create_entity();
	bare.add_component(TagComponent{ "Bare" });
	bare.add_component(TransformComponent{});

	lua["e"] = reg.get_entity(e.get_id());
	lua["bare"] = reg.get_entity(bare.get_id());

	CHECK(run(lua, R"(
		assert(e:is_valid())
		assert(e:get_id() ~= 0)
		assert(e:get_name() == "Player")
		e:set_name("Renamed")
		assert(e:get_name() == "Renamed")

		-- transform access
		local t = e:get_transform()
		assert(t ~= nil and math.abs(t.position.y - 2) < 1e-6)
		t.position = Vector3(4, 5, 6)

		-- component access; missing components come back nil
		local mat = e:get_material()
		assert(mat ~= nil and math.abs(mat.roughness - 0.5) < 1e-6)
		mat.roughness = 0.25
		mat.emission_power = 3.0
		assert(bare:get_material() == nil)

		local light = e:get_light()
		assert(light ~= nil)
		light.intensity = 7.5
		assert(bare:get_light() == nil)

		local cam = e:get_camera()
		assert(cam ~= nil)
		cam.fov = 60.0
		cam.target = Vector3(1, 1, 1)

		-- audio triggers (consumed by the audio system when one is running)
		e:play_sound()
		e:play_music()

		-- physics calls degrade to no-ops without a running simulation
		e:set_velocity(1, 2, 3)
		e:apply_impulse(0, 10, 0)
		e:set_angular_velocity(0, 1, 0)
		local v = e:get_velocity()
		assert(v.x == 0 and v.y == 0 and v.z == 0)

		-- teleport still moves the transform (the body half is the no-op)
		e:teleport(9, 8, 7)

		e:add_offset(1, 0, 0)
	)"));

	// --- verify the Lua writes landed on the C++ side ---
	CHECK(reg.try_get_component<TagComponent>(e.get_id())->name == "Renamed");

	auto* t = reg.try_get_component<TransformComponent>(e.get_id());
	CHECK(t && std::fabs(t->position.x - 10.0f) < 1e-4f); // teleport(9,..) + add_offset(1,..)
	CHECK(t && std::fabs(t->position.y - 8.0f) < 1e-4f);
	CHECK(t && t->is_dirty);

	auto* mat = reg.try_get_component<MaterialComponent>(e.get_id());
	CHECK(mat && std::fabs(mat->roughness - 0.25f) < 1e-4f);
	CHECK(mat && std::fabs(mat->emission_power - 3.0f) < 1e-4f);

	auto* light = reg.try_get_component<LightComponent>(e.get_id());
	CHECK(light && std::fabs(light->intensity - 7.5f) < 1e-4f);

	auto* cam = reg.try_get_component<CameraComponent>(e.get_id());
	CHECK(cam && std::fabs(cam->fov - 60.0f) < 1e-4f);
	CHECK(cam && std::fabs(cam->target.x - 1.0f) < 1e-4f);

	CHECK(reg.try_get_component<AudioSourceComponent>(e.get_id())->trigger_play);
	CHECK(reg.try_get_component<BackgroundMusicComponent>(e.get_id())->trigger_play);

	// --- FPS look helper: aims the camera target from yaw/pitch ---
	// The target sits far down the view ray, so test the normalized direction.
	CHECK(run(lua, R"(
		local function dir(t)
			local l = math.sqrt(t.x*t.x + t.y*t.y + t.z*t.z)
			return t.x/l, t.y/l, t.z/l
		end
		e:get_transform().position = Vector3(0, 0, 0)
		e:set_look(0, 0)                    -- yaw 0, pitch 0 -> look down +Z
		local x, y, z = dir(e:get_camera().target)
		assert(math.abs(x) < 1e-4 and math.abs(y) < 1e-4 and math.abs(z - 1) < 1e-4)
		e:set_look(90, 0)                   -- yaw 90 -> look down +X
		x, y, z = dir(e:get_camera().target)
		assert(math.abs(x - 1) < 1e-3 and math.abs(z) < 1e-3)
		e:set_look(0, 90)                   -- pitch clamps below 90, so mostly +Y
		x, y, z = dir(e:get_camera().target)
		assert(y > 0.99)
	)"));

	// --- destroyed entities turn inert, not crashy ---
	CHECK(run(lua, R"(
		bare:destroy()
		assert(not bare:is_valid())
		assert(bare:get_transform() == nil)
		assert(bare:get_name() == "")
		bare:set_velocity(1, 1, 1) -- must not blow up
	)"));
	CHECK(!reg.is_alive(bare.get_id()));

	me::scripting::shutdown();

	if (g_failures == 0) {
		std::cout << "[lua_api] PASSED\n";
		return 0;
	}
	std::cerr << "[lua_api] FAILED (" << g_failures << " check(s))\n";
	return 1;
}
