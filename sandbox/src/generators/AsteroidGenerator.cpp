#include <random>
#include <cmath>
#include <algorithm> // std::clamp

#include "AsteroidGenerator.hpp"

namespace driftspace::gen {

	float AsteroidGenerator::Distance(const me::math::Vec2& a, const me::math::Vec2& b) {
		float dx = a.x - b.x;
		float dy = a.y - b.y;
		return std::sqrt(dx * dx + dy * dy);
	}

	void AsteroidGenerator::Generate(std::vector<AsteroidBasalt>& outAsteroids, const me::math::Vec2& basePos, int seed) {
		if (m_cfg.count <= 0) return;
		if (m_cfg.outerRadius <= m_cfg.innerRadius) return;

		// ---------- RNG setup ----------
		std::mt19937 rng;
		if (seed == 0) {
			std::random_device rd;
			rng.seed(rd());
		} else {
			rng.seed(static_cast<std::mt19937::result_type>(seed));
		}

		std::uniform_real_distribution<float> distAngle(0.0f, 6.2831853f); // 0..2π
		std::uniform_real_distribution<float> distRadius(m_cfg.innerRadius, m_cfg.outerRadius);
		std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

		// Keep track of already placed asteroid positions to enforce separation
		std::vector<me::math::Vec2> placed;
		placed.reserve(m_cfg.count);

		auto tooClose = [&](float x, float y) {
			for (const auto& p : placed) {
				float dx = x - p.x;
				float dy = y - p.y;
				if (dx * dx + dy * dy < m_cfg.minSeparation * m_cfg.minSeparation)
					return true;
			}
			return false;
			};

		// We’ll use outAsteroids.size() as a base index for unique names
		int baseIndex = static_cast<int>(outAsteroids.size());

		for (int i = 0; i < m_cfg.count; ++i) {
			float x = 0.0f, y = 0.0f;
			bool ok = false;

			// Try a few times to find a non-overlapping position
			for (int attempt = 0; attempt < 16; ++attempt) {
				float angle = distAngle(rng);
				float radius = distRadius(rng);

				x = basePos.x + std::cos(angle) * radius;
				y = basePos.y + std::sin(angle) * radius;

				if (!tooClose(x, y)) {
					ok = true;
					break;
				}
			}

			if (!ok) {
				// Could not find a non-overlapping position after several attempts.
				// We still place it (last x,y) to avoid silent missing asteroids.
			}

			// Distance from base (for future rarity logic)
			me::math::Vec2 pos{ x, y };
			float distFromBase = Distance(pos, basePos);

			// ---------- Scale based on distance (simple gradient) ----------
			// t = 0 at innerRadius, t = 1 at outerRadius
			float t = (distFromBase - m_cfg.innerRadius) /
				(m_cfg.outerRadius - m_cfg.innerRadius);
			t = std::clamp(t, 0.0f, 1.0f);

			float scale = m_cfg.minScale + (m_cfg.maxScale - m_cfg.minScale) * t;

			// ---------- Spawn asteroid ----------
			AsteroidBasalt asteroid;
			asteroid.OnCreate(x, y, scale, baseIndex + i);
			outAsteroids.push_back(std::move(asteroid));

			placed.push_back(pos);
		}
	}

} // namespace driftspace::gen
