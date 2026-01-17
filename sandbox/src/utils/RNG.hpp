#pragma once

#include <cstdlib>

namespace driftspace::utils {

	inline float Rand01() {
		return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
	}

	inline float RandRange(float min, float max) {
		return min + (max - min) * Rand01();
	}

} // namespace drifstpace::utils