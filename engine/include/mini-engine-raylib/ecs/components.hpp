#pragma once

#include "mini-engine-raylib/render/color.hpp"

#include <mini-ecs/entity.hpp>

namespace me {
	using EntityId = me::entity::entity_id;
}

namespace me::components {

	struct TransformComponent {
		float x = 0.0f, y = 0.0f, z = 0.0f;
		float rot_x = 0.0f, rot_y = 0.0f, rot_z = 0.0f; // Euler angles in Degrees
		float sx = 1.0f, sy = 1.0f, sz = 1.0f;
	};

	struct CameraComponent {
		// Target is where the camera looks at
		float target_x = 0.0f, target_y = 0.0f, target_z = 0.0f;
		float up_x = 0.0f, up_y = 1.0f, up_z = 0.0f;
		float fov = 60.0f;
		int projection = 0; // 0 = Perspective (Camera3D::CAMERA_PERSPECTIVE) 1 = Orthographic
		bool active = true;
	};

	struct MeshRendererComponent {
		enum Type { Cube, Sphere, Plane } type = Cube;
		me::Color color = me::Color::white;
		bool wireframe = false;
	};

} // namespace me::components