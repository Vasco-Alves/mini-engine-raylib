#pragma once

#include <raylib.h>
#include <raymath.h>
#include <vector>
#include <algorithm> 

#include "mini-engine-raylib/render/color.hpp"
#include "mini-engine-raylib/assets/assets.hpp"

#include <mini-ecs/entity.hpp>

namespace me::components {

	struct TagComponent {
		std::string name = "Entity";
	};

	struct TransformComponent {
		Vector3 position = { 0.0f, 0.0f, 0.0f };
		Vector3 rotation = { 0.0f, 0.0f, 0.0f }; // Euler angles in Degrees (for display/inspector)
		Vector3 scale = { 1.0f, 1.0f, 1.0f };

		// Quaternion is the source of truth for rotation.
		// - The gizmo writes here directly (no Euler round-trip, no drift).
		// - The inspector writes to `rotation` (Euler), which the transform system
		//   syncs back into this quaternion when it detects a change.
		Quaternion rotation_quat = { 0.0f, 0.0f, 0.0f, 1.0f }; // Identity

		// --- HIERARCHY (SCENE GRAPH) ---
		me::entity::entity_id parent = me::entity::null;
		std::vector<me::entity::entity_id> children;

		void add_child(me::entity::entity_id child) {
			children.push_back(child);
		}

		void remove_child(me::entity::entity_id child) {
			children.erase(std::remove(children.begin(), children.end(), child), children.end());
		}

		// --- MATRIX CACHING ---
		Matrix model_matrix = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };

		Vector3 last_position = { 0.0f, 0.0f, 0.0f };
		Vector3 last_rotation = { 0.0f, 0.0f, 0.0f };
		Vector3 last_scale = { 0.0f, 0.0f, 0.0f };
		Quaternion last_rotation_quat = { 0.0f, 0.0f, 0.0f, 1.0f };

		bool is_dirty = true;
	};

	struct LightComponent {
		me::Color color = me::Color::white;
		float intensity = 1.0f;
	};

	struct DirectionalLightComponent {
		me::Color color = me::Color::white;
		float intensity = 1.0f;
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

	struct MaterialComponent {
		me::Color albedo = me::Color::white; // Base color/tint
		float roughness = 1.0f;              // 0.0 = Perfect Mirror, 1.0 = Matte/Chalk
		float metallic = 0.0f;               // 0.0 = Plastic/Wood, 1.0 = Metal
		float emission_power = 0.0f;         // Does it glow?
		// Glass & Refraction
		float transmission = 0.0f;           // 0.0 = Solid Opaque, 1.0 = Fully Transparent Glass
		float ior = 1.5f;                    // Index of Refraction (1.0=Air, 1.33=Water, 1.5=Glass, 2.4=Diamond)
	};

} // namespace me::components