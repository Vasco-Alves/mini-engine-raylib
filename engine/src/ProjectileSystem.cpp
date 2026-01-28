#include "ProjectileSystem.hpp"
#include "Components.hpp"
#include "Registry.hpp"
#include "Entity.hpp"

#include <cmath>
#include <algorithm>
#include <vector>

namespace {
	// Basic Collision Math Helpers
	struct Box { float x, y, w, h; };
	struct Circ { float cx, cy, r; };

	Box GetBox(const me::components::Transform2D& t, const me::components::AabbCollider& c) {
		return { t.x + c.ox - c.w * 0.5f, t.y + c.oy - c.h * 0.5f, c.w, c.h };
	}
	Circ GetCirc(const me::components::Transform2D& t, const me::components::CircleCollider& c) {
		return { t.x + c.ox, t.y + c.oy, c.radius };
	}
	bool Overlap(const Circ& c, const Box& b) {
		float closestX = std::max(b.x, std::min(c.cx, b.x + b.w));
		float closestY = std::max(b.y, std::min(c.cy, b.y + b.h));
		float dx = c.cx - closestX;
		float dy = c.cy - closestY;
		return (dx * dx + dy * dy) <= (c.r * c.r);
	}
}

namespace me::projectile {

	void Update(float dt) {
		auto& reg = me::detail::Reg();

		// 1. Get Pools
		auto* projPool = reg.TryGetPool<me::components::Projectile>();
		auto* hitPool = reg.TryGetPool<me::components::Hittable>(); // Optimization: Only check Hittables

		if (!projPool || !hitPool) return;

		// 2. Iterate Projectiles
		struct HitEvent { me::EntityId bullet; me::EntityId victim; };
		std::vector<HitEvent> hits;

		for (auto& pKv : projPool->data) {
			me::EntityId pId = pKv.first;
			const auto& proj = pKv.second;

			// Bullet needs Transform + Collider
			auto* pT = reg.TryGetComponent<me::components::Transform2D>(pId);
			auto* pCol = reg.TryGetComponent<me::components::AabbCollider>(pId); // Assuming bullets are AABB
			if (!pT || !pCol) continue;

			Box pBox = GetBox(*pT, *pCol);

			// Check against Hittables
			for (auto& hKv : hitPool->data) {
				me::EntityId tId = hKv.first;
				if (tId == proj.owner) continue; // Don't hit owner

				// Victim needs Transform + Collider (Circle or AABB)
				auto* tT = reg.TryGetComponent<me::components::Transform2D>(tId);
				if (!tT) continue;

				bool hit = false;

				// Case A: Victim is Circle (Asteroid)
				if (auto* tCir = reg.TryGetComponent<me::components::CircleCollider>(tId)) {
					hit = Overlap(GetCirc(*tT, *tCir), pBox);
				}
				// Case B: Victim is AABB (Base/Ship)
				else if (auto* tBox = reg.TryGetComponent<me::components::AabbCollider>(tId)) {
					// (Optional) Add AABB-AABB check here if needed
				}

				if (hit) {
					hits.push_back({ pId, tId });
					break; // Bullet hits first thing and dies
				}
			}
		}

		// 3. Resolve Hits
		for (const auto& ev : hits) {
			if (!me::IsAlive(ev.bullet) || !me::IsAlive(ev.victim)) continue;

			auto* pData = reg.TryGetComponent<me::components::Projectile>(ev.bullet);
			if (pData) {
				// Apply Damage
				if (auto* hp = reg.TryGetComponent<me::components::Health>(ev.victim)) {
					hp->current -= pData->damage;
				}
			}

			// Destroy Bullet
			me::DestroyEntity(ev.bullet);
		}
	}
}