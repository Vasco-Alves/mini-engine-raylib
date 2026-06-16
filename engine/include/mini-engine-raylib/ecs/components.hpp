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

		// World-space position. For a child this is the parent-relative `position`
		// pushed through the cached world matrix (so a camera parented to a player
		// follows it); for a root the two are identical. Roots that live outside
		// the ECS (the editor fly-cam, the game's fallback view) don't get a
		// maintained model_matrix, hence the parent check rather than always
		// reading the matrix.
		Vector3 world_position() const {
			if (parent != me::entity::null)
				return { model_matrix.m12, model_matrix.m13, model_matrix.m14 };
			return position;
		}

		Vector3 last_position = { 0.0f, 0.0f, 0.0f };
		Vector3 last_rotation = { 0.0f, 0.0f, 0.0f };
		Vector3 last_scale = { 0.0f, 0.0f, 0.0f };
		Quaternion last_rotation_quat = { 0.0f, 0.0f, 0.0f, 1.0f };

		bool is_dirty = true;
	};

	struct LightComponent {
		me::Color color = me::Color::white;
		float intensity = 1.0f;
		float radius = 0.1f; // world-space size of the light; drives soft-shadow penumbra (0 = hard)
	};

	struct DirectionalLightComponent {
		me::Color color = me::Color::white;
		float intensity = 1.0f;
		float angular_radius = 1.0f; // sun half-angle in DEGREES; drives soft-shadow penumbra (0 = hard)
		bool cast_shadows = true;    // real-time shadow map in Play/Edit (the path tracer always shadows)

		// Real-time shadow-map tuning. Extent is the world-unit width of the area
		// (centered on the camera) that receives shadows: smaller = sharper but
		// shadows end closer; resolution is the depth map size per side.
		float shadow_extent = 60.0f;
		int shadow_resolution = 2048;
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
		float tint_strength = 1.0f;          // Beer–Lambert density multiplier: 0 = always clear, 1 = physical, >1 = denser tint
	};

} // namespace me::components