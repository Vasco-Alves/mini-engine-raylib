#pragma once

#include <cstdint>

namespace me::physics2d {

	void  SetGravity(float gx, float gy);
	void  EnableWorldBounds(bool enable, float minX = 0, float minY = 0, float maxX = 0, float maxY = 0);

	void  Update(float dt);
	void StepFixed(float dt, float fixedStep, float& accumulator);
}
