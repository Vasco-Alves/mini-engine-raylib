#pragma once

#include <raylib.h>

#include "mini-engine-raylib/render/color.hpp"
#include "mini-engine-raylib/assets/assets.hpp"

#include <mini-ecs/entity.hpp>

namespace me::components {

	struct TagComponent {
		std::string name = "Entity";
	};

	struct TransformComponent {
		Vector3 position = { 0.0f, 0.0f, 0.0f };
		Vector3 rotation = { 0.0f, 0.0f, 0.0f }; // Euler angles in Degrees
		Vector3 scale = { 1.0f, 1.0f, 1.0f };

		// --- MATRIX CACHING ---
		// Starts as an Identity Matrix (no movement)
		Matrix model_matrix = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };

		Vector3 last_position = { 0.0f, 0.0f, 0.0f };
		Vector3 last_rotation = { 0.0f, 0.0f, 0.0f };
		Vector3 last_scale = { 0.0f, 0.0f, 0.0f };

		bool is_dirty = true; // Force calculate on frame 1
	};

	struct CameraComponent {
		Vector3 target = { 0.0f, 0.0f, 0.0f };
		Vector3 up = { 0.0f, 1.0f, 0.0f };
		float fov = 45.0f;
		int projection = 0; // 0 = Perspective (Camera3D::CAMERA_PERSPECTIVE) 1 = Orthographic
		bool active = true;

		float move_speed = 10.0f;
		float mouse_sens = 0.5f;
	};

	struct Camera2DComponent {
		Vector2 offset = { 0.0f, 0.0f };
		float rotation = 0.0f;
		float zoom = 1.0f;
		bool active = true;
	};

	struct Shape2DComponent {
		enum Type { Rectangle, Circle } type = Rectangle;
		me::Color color = me::Color::white;
		bool wireframe = false;
	};

	struct SpriteComponent {
		me::assets::TextureId texture{};
		me::Color tint = me::Color::white;
	};

	struct Shape3DComponent {
		enum Type { Cube, Sphere, Plane } type = Cube;
		me::Color color = me::Color::white;
		bool wireframe = false;
	};

	struct Model3DComponent {
		me::assets::ModelId model{};
		me::Color tint = me::Color::white;
	};

} // namespace me::components
