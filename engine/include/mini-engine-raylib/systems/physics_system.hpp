#pragma once

#include <mini-ecs/entity.hpp>
#include <raylib.h>
#include <vector>

namespace me {
	class Registry;
}

namespace me::physics {

	// Called once when the engine boots
	void init();

	// Called once when the engine shuts down
	void shutdown();

	// Called when the user presses PLAY. Loops through the ECS, reads components, and creates Jolt Bodies
	void on_play(me::Registry& registry);

	// Called when the user presses STOP. Destroys all Jolt bodies and resets the simulation.
	void on_stop();

	// Called every frame during Play Mode. 
	// Steps Jolt forward and writes the new positions back to the TransformComponents.
	void update(me::Registry& registry, float dt);

	// Renders the collision shapes as wireframes using Raylib's rlgl. Must be called inside a BeginMode3D() block
	void draw_debug(me::Registry& registry);

	// Creates the Jolt body for ONE entity (RigidBody + collider + Transform), for entities
	// spawned while the simulation is running. No-op outside play mode (on_play will pick the
	// entity up at the next play) or when the entity already has a body.
	void add_body(me::Registry& registry, me::entity::entity_id e);

	// Removes an entity's Jolt body, for entities destroyed (or whose RigidBody is removed)
	// while the simulation is running — without this the invisible body keeps colliding.
	// No-op outside play mode: on_stop destroys the whole physics world wholesale.
	void remove_body(me::Registry& registry, me::entity::entity_id e);

	// Pushes a RigidBody by directly setting its linear velocity (Ignores rotation)
	void set_linear_velocity(me::entity::entity_id e, float x, float y, float z);

	// Pushes a RigidBody by directly setting its linear velocity
	void set_local_linear_velocity(me::entity::entity_id e, float x, float y, float z);

	// Reads a RigidBody's current linear velocity; {0,0,0} when the entity has no live body.
	Vector3 get_linear_velocity(me::entity::entity_id e);

	// Sets a RigidBody's angular velocity (radians/second, world space).
	void set_angular_velocity(me::entity::entity_id e, float x, float y, float z);

	// Applies an instantaneous impulse (mass * velocity change) at the body's center of mass.
	void apply_impulse(me::entity::entity_id e, float x, float y, float z);

	// Moves an entity's Jolt body to a world position, keeping its velocity (the caller moves
	// the TransformComponent; this keeps the simulation in sync). No-op when not simulating.
	void teleport_body(me::entity::entity_id e, float x, float y, float z);

	// --- Collision events ---

	// One begin/end-of-contact between two entities. Jolt reports contacts from its worker
	// threads during update(); they are buffered there and handed out here afterwards, so
	// consumers (the script system) run on the main thread with the step finished.
	struct ContactEvent {
		me::entity::entity_id a = me::entity::null;
		me::entity::entity_id b = me::entity::null;
		bool entered = true; // true = contact began, false = contact ended
	};

	// Drains the events collected by the last update(). Empty when not simulating.
	std::vector<ContactEvent> consume_contact_events();

	// --- Raycasts ---

	struct RaycastHit {
		bool hit = false;
		me::entity::entity_id entity = me::entity::null;
		Vector3 point = { 0.0f, 0.0f, 0.0f };
		Vector3 normal = { 0.0f, 0.0f, 0.0f };
		float distance = 0.0f;
	};

	// Casts a ray against every live body; returns the closest hit (or .hit == false).
	// `direction` does not need to be normalized. No-op result when not simulating.
	RaycastHit raycast(Vector3 origin, Vector3 direction, float max_distance);

} // namespace me::physics