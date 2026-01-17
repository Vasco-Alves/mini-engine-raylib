#pragma once

#include <cstdint>

namespace me::physics2d {
	// Optional global gravity (down is +Y in screen space)
	void  SetGravity(float gx, float gy);
	void  EnableWorldBounds(bool enable, float minX = 0, float minY = 0, float maxX = 0, float maxY = 0);

	// Integrate velocity, resolve AABB collisions, clamp to bounds if enabled.
	void  Step(float dt);
	void StepFixed(float dt, float fixedStep, float& accumulator);
}
