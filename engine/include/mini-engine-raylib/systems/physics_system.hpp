#pragma once

#include <mini-ecs/entity.hpp>

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

	// Pushes a RigidBody by directly setting its linear velocity (Ignores rotation)
	void set_linear_velocity(me::entity::entity_id e, float x, float y, float z);

	// Pushes a RigidBody by directly setting its linear velocity
	void set_local_linear_velocity(me::entity::entity_id e, float x, float y, float z);

} // namespace me::physics