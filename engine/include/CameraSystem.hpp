#pragma once

#include "Entity.hpp"

namespace me::camera {
	// Updates any entity with both Transform and Camera components
	// based on "LookX"/"LookY" and "MoveX"/"MoveZ"/"MoveY" inputs.
	void UpdateFreeFly(float dt);

	// Instantly rotates the camera entity to look at the target (x,y,z)
	void LookAt(me::EntityId camEnt, float x, float y, float z);
}