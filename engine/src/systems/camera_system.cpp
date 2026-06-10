#include <cmath>
#include <algorithm>

#include "mini-engine-raylib/core/engine.hpp"
#include "mini-engine-raylib/input/input.hpp"
#include "mini-engine-raylib/systems/camera_system.hpp"
#include "mini-engine-raylib/ecs/components.hpp"

#include <mini-ecs/registry.hpp>

namespace me::camera {

	void update_editor_camera(me::components::TransformComponent& t, me::components::CameraComponent& cam, float dt) {
		t.rotation.y -= me::input::axis_value("LookX") * cam.mouse_sens;	// Horizontal mouse turns Y axis (Yaw)
		t.rotation.x -= me::input::axis_value("LookY") * cam.mouse_sens;	// Vertical mouse turns X axis (Pitch)
		t.rotation.x = std::clamp(t.rotation.x, -89.0f, 89.0f);				// Clamp the X axis so you don't do backflips

		float yaw_rad = t.rotation.y * (PI / 180.0f);
		float pitch_rad = t.rotation.x * (PI / 180.0f);

		float look_x = std::sin(yaw_rad) * std::cos(pitch_rad);
		float look_y = std::sin(pitch_rad);
		float look_z = std::cos(yaw_rad) * std::cos(pitch_rad);

		float fwd_x = std::sin(yaw_rad);
		float fwd_z = std::cos(yaw_rad);
		float right_x = std::cos(yaw_rad);
		float right_z = -std::sin(yaw_rad);

		float move_forward = -me::input::axis_value("MoveZ");
		float move_strafe = -me::input::axis_value("MoveX");
		float move_up = me::input::axis_value("MoveY");

		// Speed modifiers: Shift = sprint, Ctrl = precision placement.
		float speed = cam.move_speed;
		if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))     speed *= 3.0f;
		if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) speed *= 0.2f;

		t.position.x += (fwd_x * move_forward + right_x * move_strafe) * speed * dt;
		t.position.z += (fwd_z * move_forward + right_z * move_strafe) * speed * dt; // Forward/Strafe affects Z
		t.position.y += (move_up * speed * dt);                                      // Up/Down affects Y

		cam.target.x = t.position.x + look_x;
		cam.target.y = t.position.y + look_y;
		cam.target.z = t.position.z + look_z;
	}

	void update_free_fly(float dt) {
		auto& reg = me::get_registry();
		auto& cam_pool = reg.view<me::components::CameraComponent>();

		for (size_t i = 0; i < cam_pool.size(); ++i) {
			me::entity::entity_id e = cam_pool.entity_map[i];
			auto& cam = cam_pool.components[i];

			if (!cam.active) continue;

			auto* t = reg.try_get_component<me::components::TransformComponent>(e);
			if (!t) continue;

			// Call the new shared math function
			update_editor_camera(*t, cam, dt);
		}
	}

	void look_at(me::entity::entity_id cam_ent, float target_x, float target_y, float target_z) {
		auto& reg = me::get_registry();
		auto* t = reg.try_get_component<me::components::TransformComponent>(cam_ent);

		if (!t) return;

		float dx = target_x - t->position.x;
		float dy = target_y - t->position.y;
		float dz = target_z - t->position.z;

		t->rotation.y = std::atan2(dx, dz) * (180.0f / PI);
		float horiz_dist = std::sqrt(dx * dx + dz * dz);
		t->rotation.x = std::atan2(dy, horiz_dist) * (180.0f / PI);
	}

	void orbit_editor_camera(me::components::TransformComponent& t, me::components::CameraComponent& cam, const Vector3& orbit_target, float dt) {
		// Rotate yaw/pitch from mouse delta
		t.rotation.y -= me::input::axis_value("LookX") * cam.mouse_sens;
		t.rotation.x -= me::input::axis_value("LookY") * cam.mouse_sens;
		t.rotation.x = std::clamp(t.rotation.x, -89.0f, 89.0f);

		float yaw_rad = t.rotation.y * (PI / 180.0f);
		float pitch_rad = t.rotation.x * (PI / 180.0f);

		// Forward vector from current angles
		float look_x = std::sin(yaw_rad) * std::cos(pitch_rad);
		float look_y = std::sin(pitch_rad);
		float look_z = std::cos(yaw_rad) * std::cos(pitch_rad);

		// Distance from camera to the stable orbit target
		float dx = t.position.x - orbit_target.x;
		float dy = t.position.y - orbit_target.y;
		float dz = t.position.z - orbit_target.z;
		float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
		if (dist < 0.5f) dist = 0.5f;

		// Reposition camera on the sphere around the orbit target
		t.position.x = orbit_target.x - look_x * dist;
		t.position.y = orbit_target.y - look_y * dist;
		t.position.z = orbit_target.z - look_z * dist;

		// Update the raylib target to stay in front of the camera
		cam.target.x = t.position.x + look_x;
		cam.target.y = t.position.y + look_y;
		cam.target.z = t.position.z + look_z;
	}

}
