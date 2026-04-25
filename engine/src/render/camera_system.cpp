#include "mini-engine-raylib/render/camera_system.hpp"
#include "mini-engine-raylib/ecs/components.hpp"
#include "mini-engine-raylib/core/engine.hpp"
#include "mini-engine-raylib/input/input.hpp"
#include "mini-engine-raylib/core/math.hpp"

#include <mini-ecs/registry.hpp>

#include <cmath>
#include <algorithm>

namespace me::camera {

	void update_free_fly(float dt) {
		auto& reg = me::get_registry();
		auto& cam_pool = reg.view<me::components::CameraComponent>();

		const float mouse_sens = 0.5f;
		const float move_speed = 10.0f;

		// Iterate sparse set
		for (size_t i = 0; i < cam_pool.size(); ++i) {
			me::entity::entity_id e = cam_pool.entity_map[i];
			auto& cam = cam_pool.components[i];

			if (!cam.active) continue;

			auto* t = reg.try_get_component<me::components::TransformComponent>(e);
			if (!t) continue;

			// --- 1. Rotation ---
			t->rot_y -= me::input::axis_value("LookX") * mouse_sens;
			t->rot_x -= me::input::axis_value("LookY") * mouse_sens;
			t->rot_x = std::clamp(t->rot_x, -89.0f, 89.0f);

			float yaw_rad = t->rot_y * (me::math::pi / 180.0f);
			float pitch_rad = t->rot_x * (me::math::pi / 180.0f);

			// --- 2. Calculate Look Direction ---
			float look_x = std::sin(yaw_rad) * std::cos(pitch_rad);
			float look_y = std::sin(pitch_rad);
			float look_z = std::cos(yaw_rad) * std::cos(pitch_rad);

			// --- 3. Calculate Movement Vectors (Flat) ---
			float fwd_x = std::sin(yaw_rad);
			float fwd_z = std::cos(yaw_rad);
			float right_x = std::cos(yaw_rad);
			float right_z = -std::sin(yaw_rad);

			// --- 4. Inputs ---
			float move_forward = -me::input::axis_value("MoveZ");
			float move_strafe = -me::input::axis_value("MoveX");
			float move_up = me::input::axis_value("MoveY");

			// --- 5. Apply Movement ---
			t->x += (fwd_x * move_forward + right_x * move_strafe) * move_speed * dt;
			t->z += (fwd_z * move_forward + right_z * move_strafe) * move_speed * dt;
			t->y += (move_up * move_speed * dt);

			// --- 6. Update Target ---
			cam.target_x = t->x + look_x;
			cam.target_y = t->y + look_y;
			cam.target_z = t->z + look_z;
		}
	}

	void look_at(me::entity::entity_id cam_ent, float target_x, float target_y, float target_z) {
		auto& reg = me::get_registry();
		auto* t = reg.try_get_component<me::components::TransformComponent>(cam_ent);

		if (!t) return;

		float dx = target_x - t->x;
		float dy = target_y - t->y;
		float dz = target_z - t->z;

		t->rot_y = std::atan2(dx, dz) * (180.0f / me::math::pi);

		float horiz_dist = std::sqrt(dx * dx + dz * dz);
		t->rot_x = std::atan2(dy, horiz_dist) * (180.0f / me::math::pi);
	}
}