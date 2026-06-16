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

		// Sensor body: overlaps fire collision callbacks but produce no physical
		// response (pickups, damage zones, level-exit volumes).
		bool is_trigger = false;

		// Lock rotation about a world axis. An upright character freezes X and Z so
		// it can't tip over while walking (leave Y free to spin in place, or freeze
		// it too to pin the facing). Dynamic bodies only; applied at body creation.
		bool freeze_rot_x = false;
		bool freeze_rot_y = false;
		bool freeze_rot_z = false;

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