#pragma once

#include "mini-engine-raylib/ecs/components.hpp"

namespace me::camera {
	// Updates any entity with both Transform and Camera components
	// based on "LookX"/"LookY" and "MoveX"/"MoveZ"/"MoveY" inputs.
	void update_free_fly(float dt);

	// Instantly rotates the camera entity to look at the target (x,y,z)
	void look_at(me::EntityId cam_ent, float x, float y, float z);
}