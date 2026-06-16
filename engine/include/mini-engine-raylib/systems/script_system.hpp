#pragma once

namespace me::systems {

	void script_update(float dt);

	// Drains the contact events buffered by the physics step and calls the
	// involved entities' on_collision_enter / on_collision_exit script
	// callbacks. Runs right after physics::update in me::world_update.
	void script_dispatch_collisions();

} // namespace me::systems
