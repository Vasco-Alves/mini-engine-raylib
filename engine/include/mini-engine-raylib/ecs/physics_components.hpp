#pragma once

#include <raylib.h>

namespace me::components {

	enum class RigidBodyType {
		Static,  // For floors and walls (never moves, infinite mass)
		Dynamic, // For cubes and players (falls with gravity, bounces)
		Kinematic // Moves only when explicitly told to via code (e.g., moving platforms)
	};

	struct RigidBodyComponent {
		RigidBodyType type = RigidBodyType::Dynamic;
		float mass = 1.0f;
		float bounciness = 0.2f;
		float friction = 0.5f;

		// Store Jolt BodyID
		uint32_t runtime_body_id = 0xFFFFFFFF;
	};

	struct BoxColliderComponent {
		Vector3 half_extents = { 1.0f, 1.0f, 1.0f };
		bool show_debug = true;
	};

	struct SphereColliderComponent {
		float radius = 1.0f;
		bool show_debug = true;
	};

} // me::components