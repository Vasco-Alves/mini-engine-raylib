#pragma once

#include "Entity.hpp"

#include <cstdint>
#include <vector>

namespace me::physics {

	struct Collision {
		me::EntityId a = 0;
		me::EntityId b = 0;
	};

	void  SetGravity(float gx, float gy);
	void  EnableWorldBounds(bool enable, float minX = 0, float minY = 0, float maxX = 0, float maxY = 0);

	// Retrieve all collision pairs that occurred during the last Update()
	const std::vector<Collision>& GetCollisions();

	void Update(float dt);
	void StepFixed(float dt, float fixedStep, float& accumulator);

}