#include "Physics2D.hpp"
#include "Entity.hpp"
#include "Components.hpp"
#include "Registry.hpp" // Now required for template methods

#include <raylib.h>

#include <vector>
#include <algorithm>
#include <cmath>

namespace {

	struct AABB { float x, y, w, h; };

	inline AABB MakeAabb(const me::components::Transform2D& t, const me::components::AabbCollider& c) {
		// Center of the collider in world space
		float cx = t.x + c.ox;
		float cy = t.y + c.oy;

		// Convert to top-left for AABB math
		float x = cx - c.w * 0.5f;
		float y = cy - c.h * 0.5f;

		return AABB{ x, y, c.w, c.h };
	}

	struct CircleShape {
		float cx;
		float cy;
		float r;
	};

	inline CircleShape MakeCircle(
		const me::components::Transform2D& t,
		const me::components::CircleCollider& c) {
		// Center of the circle in world space
		float cx = t.x + c.ox;
		float cy = t.y + c.oy;

		// Return circle defined by world-space center and radius
		return CircleShape{ cx, cy, c.radius };
	}

	inline bool OverlapAabbAabb(const AABB& a, const AABB& b, float& outDx, float& outDy) {
		float dx = (a.x + a.w * 0.5f) - (b.x + b.w * 0.5f);
		float px = (a.w + b.w) * 0.5f - std::fabs(dx);
		if (px <= 0) return false;

		float dy = (a.y + a.h * 0.5f) - (b.y + b.h * 0.5f);
		float py = (a.h + b.h) * 0.5f - std::fabs(dy);
		if (py <= 0) return false;

		if (px < py) {
			outDx = (dx < 0) ? -px : px;
			outDy = 0.0f;
		} else {
			outDx = 0.0f;
			outDy = (dy < 0) ? -py : py;
		}
		return true;
	}

	// Pushes the CIRCLE out of the AABB by (outDx, outDy)
	inline bool OverlapCircleAabb(const CircleShape& c, const AABB& b, float& outDx, float& outDy) {
		float closestX = std::fmax(b.x, std::fmin(c.cx, b.x + b.w));
		float closestY = std::fmax(b.y, std::fmin(c.cy, b.y + b.h));

		float dx = c.cx - closestX;
		float dy = c.cy - closestY;
		float dist2 = dx * dx + dy * dy;
		float r2 = c.r * c.r;

		if (dist2 >= r2) return false;

		// Handle the degenerate case where the closest point is exactly the center
		if (dist2 <= 1e-8f) {
			float left = c.cx - b.x;
			float right = (b.x + b.w) - c.cx;
			float top = c.cy - b.y;
			float bottom = (b.y + b.h) - c.cy;

			float minSide = left;
			outDx = -(c.r + left);
			outDy = 0.0f;

			if (right < minSide) {
				minSide = right;
				outDx = (c.r + right);
				outDy = 0.0f;
			}
			if (top < minSide) {
				minSide = top;
				outDx = 0.0f;
				outDy = -(c.r + top);
			}
			if (bottom < minSide) {
				minSide = bottom;
				outDx = 0.0f;
				outDy = (c.r + bottom);
			}

			return true;
		}

		float dist = std::sqrt(dist2);
		float penetration = c.r - dist;
		if (penetration <= 0.0f) return false;

		float nx = dx / dist;
		float ny = dy / dist;

		outDx = nx * penetration;
		outDy = ny * penetration;
		return true;
	}

	inline bool OverlapCircleCircle(const CircleShape& a, const CircleShape& b, float& outDx, float& outDy) {
		float dx = a.cx - b.cx;
		float dy = a.cy - b.cy;
		float dist2 = dx * dx + dy * dy;
		float rSum = a.r + b.r;
		float rSum2 = rSum * rSum;

		if (dist2 >= rSum2) return false;

		if (dist2 == 0.0f) {
			outDx = rSum;
			outDy = 0.0f;
			return true;
		}

		float dist = std::sqrt(dist2);
		float penetration = rSum - dist;
		if (penetration <= 0.0f) return false;

		float nx = dx / dist;
		float ny = dy / dist;

		outDx = nx * penetration;
		outDy = ny * penetration;
		return true;
	}

	struct MassInfo {
		float mass = 1.0f;
		float invMass = 1.0f;
	};

	inline MassInfo ComputeMass(bool hasAabb, const me::components::AabbCollider* aabb,
		bool hasCircle, const me::components::CircleCollider* circle) {
		float m = 1.0f;

		if (hasCircle && circle) {
			float r = std::max(circle->radius, 1.0f);
			m = r * r;
		} else if (hasAabb && aabb) {
			float area = std::max(aabb->w * aabb->h, 16.0f);
			m = area;
		}

		if (m < 10.0f) m = 10.0f;
		if (m > 50000.0f) m = 50000.0f;

		MassInfo mi;
		mi.mass = m;
		mi.invMass = 1.0f / m;
		return mi;
	}

	float g_gx = 0.0f, g_gy = 0.0f;
	bool  g_boundsEnabled = false;
	float g_minX = 0, g_minY = 0, g_maxX = 0, g_maxY = 0;
}

namespace me::physics2d {

	void SetGravity(float gx, float gy) { g_gx = gx; g_gy = gy; }

	void EnableWorldBounds(bool enable, float minX, float minY, float maxX, float maxY) {
		g_boundsEnabled = enable;
		g_minX = minX; g_minY = minY; g_maxX = maxX; g_maxY = maxY;
	}

	void Update(float dt) {
		auto& reg = me::detail::Reg();

		// We need the Velocity pool to find dynamic bodies
		auto* velPool = reg.TryGetPool<me::components::Velocity2D>();
		if (!velPool) return; // No physics bodies

		struct Body {
			me::EntityId e;
			me::components::Transform2D* t;
			me::components::Velocity2D* v;
			me::components::AabbCollider* aabb = nullptr;
			me::components::CircleCollider* circle = nullptr;
			MassInfo massInfo{};
		};

		std::vector<Body> bodies;
		bodies.reserve(velPool->data.size());

		// 1. Gather Dynamic Bodies (Entities with Velocity + Transform)
		for (auto& kv : velPool->data) {
			me::EntityId e = kv.first;
			me::components::Velocity2D& vel = kv.second;

			auto* t = reg.TryGetComponent<me::components::Transform2D>(e);
			if (!t) continue;

			Body b{};
			b.e = e;
			b.t = t;
			b.v = &vel;

			b.aabb = reg.TryGetComponent<me::components::AabbCollider>(e);
			b.circle = reg.TryGetComponent<me::components::CircleCollider>(e);

			// Mass calculation
			if (vel.mass > 0.0f) {
				float m = vel.mass;
				if (m < 0.1f)  m = 0.1f;
				if (m > 1e6f)  m = 1e6f;
				b.massInfo.mass = m;
				b.massInfo.invMass = 1.0f / m;
			} else {
				b.massInfo = ComputeMass(b.aabb != nullptr, b.aabb, b.circle != nullptr, b.circle);
			}

			bodies.push_back(b);
		}

		// 2. Gather Static Bodies (Colliders WITHOUT Velocity)
		// We need pointers to data, so we grab from the pools directly.
		struct StaticAabb {
			me::components::Transform2D* t;
			me::components::AabbCollider* c;
		};
		struct StaticCircle {
			me::components::Transform2D* t;
			me::components::CircleCollider* c;
		};

		std::vector<StaticAabb> staticsAabb;
		std::vector<StaticCircle> staticsCircle;

		if (auto* aabbPool = reg.TryGetPool<me::components::AabbCollider>()) {
			for (auto& kv : aabbPool->data) {
				me::EntityId e = kv.first;
				// If it has velocity, we already handled it as dynamic
				if (reg.HasComponent<me::components::Velocity2D>(e)) continue;

				auto* t = reg.TryGetComponent<me::components::Transform2D>(e);
				if (t) {
					staticsAabb.push_back({ t, &kv.second });
				}
			}
		}

		if (auto* circlePool = reg.TryGetPool<me::components::CircleCollider>()) {
			for (auto& kv : circlePool->data) {
				me::EntityId e = kv.first;
				if (reg.HasComponent<me::components::Velocity2D>(e)) continue;

				auto* t = reg.TryGetComponent<me::components::Transform2D>(e);
				if (t) {
					staticsCircle.push_back({ t, &kv.second });
				}
			}
		}

		// 3. Integrate velocities (+ gravity)
		for (auto& b : bodies) {
			b.v->vx += g_gx * dt;
			b.v->vy += g_gy * dt;
			b.t->x += b.v->vx * dt;
			b.t->y += b.v->vy * dt;
		}

		// 4. Resolve against static colliders
		for (auto& b : bodies) {
			if (b.aabb && b.aabb->solid) {
				AABB a = MakeAabb(*b.t, *b.aabb);

				// AABB vs static AABBs
				for (const auto& s : staticsAabb) {
					if (!s.c->solid) continue;
					AABB sb = MakeAabb(*s.t, *s.c);
					float pushX = 0, pushY = 0;
					if (OverlapAabbAabb(a, sb, pushX, pushY)) {
						b.t->x += pushX;
						b.t->y += pushY;
						if (pushX != 0.0f) b.v->vx = 0.0f;
						if (pushY != 0.0f) b.v->vy = 0.0f;
						a = MakeAabb(*b.t, *b.aabb);
					}
				}

				// AABB vs static Circles
				for (const auto& s : staticsCircle) {
					if (!s.c->solid) continue;
					CircleShape sc = MakeCircle(*s.t, *s.c);
					float pushX = 0, pushY = 0;
					if (OverlapCircleAabb(sc, a, pushX, pushY)) {
						b.t->x -= pushX;
						b.t->y -= pushY;
						if (std::fabs(pushX) > std::fabs(pushY)) b.v->vx = 0.0f;
						else b.v->vy = 0.0f;
						a = MakeAabb(*b.t, *b.aabb);
					}
				}
			} else if (b.circle && b.circle->solid) {
				CircleShape c = MakeCircle(*b.t, *b.circle);

				// Circle vs static AABBs
				for (const auto& s : staticsAabb) {
					if (!s.c->solid) continue;
					AABB sb = MakeAabb(*s.t, *s.c);
					float pushX = 0, pushY = 0;
					if (OverlapCircleAabb(c, sb, pushX, pushY)) {
						b.t->x += pushX;
						b.t->y += pushY;
						if (std::fabs(pushX) > std::fabs(pushY)) b.v->vx = 0.0f;
						else b.v->vy = 0.0f;
						c = MakeCircle(*b.t, *b.circle);
					}
				}

				// Circle vs static Circles
				for (const auto& s : staticsCircle) {
					if (!s.c->solid) continue;
					CircleShape sc = MakeCircle(*s.t, *s.c);
					float pushX = 0, pushY = 0;
					if (OverlapCircleCircle(c, sc, pushX, pushY)) {
						b.t->x += pushX;
						b.t->y += pushY;
						if (std::fabs(pushX) > std::fabs(pushY)) b.v->vx = 0.0f;
						else b.v->vy = 0.0f;
						c = MakeCircle(*b.t, *b.circle);
					}
				}
			}
		}

		// 5. Dynamic-Dynamic Collisions
		const std::size_t n = bodies.size();
		for (std::size_t i = 0; i < n; ++i) {
			for (std::size_t j = i + 1; j < n; ++j) {
				Body& A = bodies[i];
				Body& B = bodies[j];

				bool solidA = (A.aabb && A.aabb->solid) || (A.circle && A.circle->solid);
				bool solidB = (B.aabb && B.aabb->solid) || (B.circle && B.circle->solid);

				if (!solidA || !solidB) continue;

				float dx = 0.0f, dy = 0.0f;
				bool collided = false;

				// Collision Checks
				if (A.circle && B.circle) {
					CircleShape ca = MakeCircle(*A.t, *A.circle);
					CircleShape cb = MakeCircle(*B.t, *B.circle);
					collided = OverlapCircleCircle(ca, cb, dx, dy);
				} else if (A.circle && B.aabb) {
					CircleShape ca = MakeCircle(*A.t, *A.circle);
					AABB bb = MakeAabb(*B.t, *B.aabb);
					collided = OverlapCircleAabb(ca, bb, dx, dy);
				} else if (A.aabb && B.circle) {
					CircleShape cb = MakeCircle(*B.t, *B.circle);
					AABB aa = MakeAabb(*A.t, *A.aabb);
					collided = OverlapCircleAabb(cb, aa, dx, dy);
					if (collided) { dx = -dx; dy = -dy; }
				} else if (A.aabb && B.aabb) {
					AABB aa = MakeAabb(*A.t, *A.aabb);
					AABB bb = MakeAabb(*B.t, *B.aabb);
					collided = OverlapAabbAabb(aa, bb, dx, dy);
				}

				if (!collided) continue;

				// Response
				float penLen2 = dx * dx + dy * dy;
				if (penLen2 <= 0.0f) continue;
				float penLen = std::sqrt(penLen2);
				float nx = dx / penLen;
				float ny = dy / penLen;

				float invA = A.massInfo.invMass;
				float invB = B.massInfo.invMass;
				float invSum = invA + invB;
				if (invSum <= 0.0f) invSum = 1.0f;

				// Position Correction
				float moveA = (invA / invSum) * penLen;
				float moveB = (invB / invSum) * penLen;

				A.t->x += nx * moveA;
				A.t->y += ny * moveA;
				B.t->x -= nx * moveB;
				B.t->y -= ny * moveB;

				// Velocity Impulse
				float relVx = B.v->vx - A.v->vx;
				float relVy = B.v->vy - A.v->vy;
				float relAlongN = relVx * nx + relVy * ny;

				if (relAlongN <= 0.0f) continue;

				float restitution = 0.0f;
				float jj = -(1.0f + restitution) * relAlongN / invSum;
				float impulseX = jj * nx;
				float impulseY = jj * ny;

				A.v->vx -= impulseX * invA;
				A.v->vy -= impulseY * invA;
				B.v->vx += impulseX * invB;
				B.v->vy += impulseY * invB;
			}
		}

		// 6. World Bounds
		if (g_boundsEnabled) {
			for (auto& b : bodies) {
				if (b.aabb) {
					AABB a = MakeAabb(*b.t, *b.aabb);
					float nx = b.t->x, ny = b.t->y;

					if (a.x < g_minX) { nx += (g_minX - a.x); b.v->vx = 0.0f; }
					if (a.y < g_minY) { ny += (g_minY - a.y); b.v->vy = 0.0f; }
					if (a.x + a.w > g_maxX) { nx -= (a.x + a.w - g_maxX); b.v->vx = 0.0f; }
					if (a.y + a.h > g_maxY) { ny -= (a.y + a.h - g_maxY); b.v->vy = 0.0f; }

					b.t->x = nx; b.t->y = ny;
				} else if (b.circle) {
					CircleShape c = MakeCircle(*b.t, *b.circle);
					float nx = b.t->x, ny = b.t->y;
					float left = c.cx - c.r;
					float right = c.cx + c.r;
					float top = c.cy - c.r;
					float bottom = c.cy + c.r;

					if (left < g_minX) { nx += (g_minX - left); b.v->vx = 0.0f; }
					if (top < g_minY) { ny += (g_minY - top); b.v->vy = 0.0f; }
					if (right > g_maxX) { nx -= (right - g_maxX); b.v->vx = 0.0f; }
					if (bottom > g_maxY) { ny -= (bottom - g_maxY); b.v->vy = 0.0f; }

					b.t->x = nx; b.t->y = ny;
				}
			}
		}
	}

	void StepFixed(float dt, float fixedStep, float& acc) {
		if (fixedStep <= 0.0f) { Update(dt); return; }
		const int maxSteps = 5;
		acc += dt;
		int steps = 0;
		while (acc >= fixedStep && steps < maxSteps) {
			Update(fixedStep);
			acc -= fixedStep;
			++steps;
		}
		if (steps == maxSteps) acc = 0.0f;
	}

}

