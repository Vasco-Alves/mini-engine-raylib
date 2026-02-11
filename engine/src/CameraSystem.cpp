#include "CameraSystem.hpp"
#include "Components.hpp"
#include "Registry.hpp"
#include "Input.hpp"

#include <cmath>
#include <algorithm>

namespace me::camera {

	void UpdateFreeFly(float dt) {
		auto& reg = me::detail::Reg();
		auto* camPool = reg.TryGetPool<me::components::Camera>();
		if (!camPool) return;

		const float mouseSens = 0.5f;
		const float moveSpeed = 10.0f;

		for (auto& kv : camPool->data) {
			me::EntityId e = kv.first;
			me::components::Camera& cam = kv.second;

			if (!cam.active) continue;

			auto* t = reg.TryGetComponent<me::components::Transform>(e);
			if (!t) continue;

			// --- 1. Rotation ---
			t->rotY -= me::input::AxisValue("LookX") * mouseSens;
			t->rotX -= me::input::AxisValue("LookY") * mouseSens;
			t->rotX = std::clamp(t->rotX, -89.0f, 89.0f);

			float yawRad = t->rotY * (me::math::Pi / 180.0f);
			float pitchRad = t->rotX * (me::math::Pi / 180.0f);

			// --- 2. Calculate Look Direction ---
			float lookX = std::sin(yawRad) * std::cos(pitchRad);
			float lookY = std::sin(pitchRad);
			float lookZ = std::cos(yawRad) * std::cos(pitchRad);

			// --- 3. Calculate Movement Vectors (Flat) ---
			// We use only Yaw for movement to stay parallel to the floor
			float fwdX = std::sin(yawRad);
			float fwdZ = std::cos(yawRad);

			float rightX = std::cos(yawRad);
			float rightZ = -std::sin(yawRad);

			// --- 4. Inputs ---
			// Negate MoveZ so W (positive) moves forward along +Z vector
			float moveForward = -me::input::AxisValue("MoveZ");
			float moveStrafe = -me::input::AxisValue("MoveX");
			float moveUp = me::input::AxisValue("MoveY");

			// --- 5. Apply Movement ---
			t->x += (fwdX * moveForward + rightX * moveStrafe) * moveSpeed * dt;
			t->z += (fwdZ * moveForward + rightZ * moveStrafe) * moveSpeed * dt;
			t->y += (moveUp * moveSpeed * dt);

			// --- 6. Update Target ---
			cam.targetX = t->x + lookX;
			cam.targetY = t->y + lookY;
			cam.targetZ = t->z + lookZ;
		}
	}

	void LookAt(me::EntityId camEnt, float targetX, float targetY, float targetZ) {
		auto& reg = me::detail::Reg();
		auto* t = reg.TryGetComponent<me::components::Transform>(camEnt);

		if (!t) return;

		// 1. Calculate Direction Vector
		float dx = targetX - t->x;
		float dy = targetY - t->y;
		float dz = targetZ - t->z;

		// 2. Calculate Yaw (Rotation around Y-axis)
		// Our math uses: x = sin(yaw), z = cos(yaw)
		// So tan(yaw) = x / z  =>  yaw = atan2(x, z)
		t->rotY = std::atan2(dx, dz) * (180.0f / me::math::Pi);

		// 3. Calculate Pitch (Rotation around X-axis)
		// Distance on the flat XZ plane
		float horizDist = std::sqrt(dx * dx + dz * dz);

		// tan(pitch) = y / horizDist  =>  pitch = atan2(y, horizDist)
		t->rotX = std::atan2(dy, horizDist) * (180.0f / me::math::Pi);
	}
}