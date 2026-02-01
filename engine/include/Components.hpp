#pragma once

#include "Assets.hpp"
#include "Color.hpp"
#include "Audio.hpp" 

namespace me { using EntityId = std::uint32_t; }

namespace me::components {

	struct Transform {
		float x = 0.0f, y = 0.0f, z = 0.0f;
		float rotX = 0.0f, rotY = 0.0f, rotZ = 0.0f; // Euler angles in Degrees
		float sx = 1.0f, sy = 1.0f, sz = 1.0f;
	};

	struct Camera {
		// Target is where the camera looks at
		float targetX = 0.0f, targetY = 0.0f, targetZ = 0.0f;
		float upX = 0.0f, upY = 1.0f, upZ = 0.0f;
		float fov = 60.0f;
		int projection = 0; // 0 = Perspective (Camera3D::CAMERA_PERSPECTIVE) 1 = Orthographic
		bool active = true;
	};

	// 3. Render 3D Primitives (later Models)
	struct MeshRenderer {
		enum Type { Cube, Sphere, Plane } type = Cube;
		me::Color color = me::Color::White;
		bool wireframe = false;
	};

	struct Velocity {
		float vx = 0.0f, vy = 0.0f, vz = 0.0f;
	};

	// --- Utilities ---
	struct Lifetime {
		float remaining = 1.0f;
	};

} // namespace me::components