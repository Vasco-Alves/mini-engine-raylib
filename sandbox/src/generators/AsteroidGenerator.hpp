#pragma once

#include <vector>

#include "Math.hpp"

#include "../objects/AsteroidBasalt.hpp"

// You can change the namespace name if you prefer something else.
namespace driftspace::gen {

	struct AsteroidGeneratorConfig {
		int   count = 40;      // how many asteroids to spawn
		float innerRadius = 2000.0f; // min distance from base
		float outerRadius = 6000.0f; // max distance from base
		float minSeparation = 600.0f;  // minimum distance between asteroid centers

		// For future tuning
		float minScale = 0.5f;
		float maxScale = 1.7f;
	};

	class AsteroidGenerator {
	public:
		explicit AsteroidGenerator(const AsteroidGeneratorConfig& cfg = {})
			: m_cfg(cfg) {
		}

		// basePos: world position of the main base (or origin)
		// outAsteroids: we create AsteroidBasalt instances and call OnCreate on them.
		// seed: optional; pass 0 to use a random seed.
		void Generate(std::vector<AsteroidBasalt>& outAsteroids, const me::math::Vec2& basePos, int seed = 0);

	private:
		AsteroidGeneratorConfig m_cfg;

		// For future: distance-based rarity hooks (currently just used for scaling)
		float Distance(const me::math::Vec2& a, const me::math::Vec2& b);
	};

} // namespace driftspace::gen
