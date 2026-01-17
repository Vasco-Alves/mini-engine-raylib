#include "Physics2D.hpp"
#include "Entity.hpp"
#include "Components.hpp"
#include "ComponentsInternal.hpp"

#include <raylib.h>

#include <vector>
#include <algorithm>
#include <cmath>

namespace {

	struct AABB { float x, y, w, h; };

	/*inline AABB MakeAabb(const me::components::Transform2D& t, const me::components::AabbCollider& c) {
		return AABB{ t.x + c.ox, t.y + c.oy, c.w, c.h };
	}*/

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

	/*inline CircleShape MakeCircle(const me::components::Transform2D& t, const me::components::CircleCollider& c) {
		return CircleShape{ t.x + c.ox, t.y + c.oy, c.radius };
	}*/

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

		// Handle the degenerate case where the closest point is exactly the center,
		// i.e. the center is inside the box (or numerically extremely close).
		if (dist2 <= 1e-8f) {
			// Distances from center to each side of the box
			float left = c.cx - b.x;
			float right = (b.x + b.w) - c.cx;
			float top = c.cy - b.y;
			float bottom = (b.y + b.h) - c.cy;

			// Choose the shallowest side and push the circle completely outside
			// that side, taking its radius into account.
			float minSide = left;
			outDx = -(c.r + left);  // move center to x = b.x - r
			outDy = 0.0f;

			if (right < minSide) {
				minSide = right;
				outDx = (c.r + right); // move center to x = b.x + b.w + r
				outDy = 0.0f;
			}
			if (top < minSide) {
				minSide = top;
				outDx = 0.0f;
				outDy = -(c.r + top);  // move center to y = b.y - r
			}
			if (bottom < minSide) {
				minSide = bottom;
				outDx = 0.0f;
				outDy = (c.r + bottom); // move center to y = b.y + b.h + r
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
			// Same center; push arbitrarily on X
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

	// Simple mass heuristic based on size.
	// We keep this internal so the engine can later expose a proper PhysicsBody component if needed.
	struct MassInfo {
		float mass = 1.0f;
		float invMass = 1.0f;
	};

	inline MassInfo ComputeMass(bool hasAabb, const me::components::AabbCollider& aabb,
		bool hasCircle, const me::components::CircleCollider& circle) {
		float m = 1.0f;

		if (hasCircle) {
			// mass ~ area ~ r^2
			float r = std::max(circle.radius, 1.0f);
			m = r * r;
		} else if (hasAabb) {
			// mass ~ area
			float area = std::max(aabb.w * aabb.h, 16.0f);
			m = area;
		} else {
			m = 1.0f;
		}

		// Clamp to avoid insane values
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

	void Step(float dt) {
		struct Body {
			me::EntityId e;
			me::components::Transform2D t;
			me::components::Velocity2D v;
			bool hasAabb = false;
			me::components::AabbCollider aabb{};
			bool hasCircle = false;
			me::components::CircleCollider circle{};
			MassInfo massInfo{};
		};

		std::vector<Body> bodies;
		bodies.reserve(128);

		// Dynamic bodies = entities with Velocity2D + Transform2D
		me::detail::ForEachVelocity([&](me::EntityId e, const me::components::Velocity2D& vel) {
			me::components::Transform2D t{};
			if (!me::GetComponent(e, t)) return;

			Body b{};
			b.e = e;
			b.t = t;
			b.v = vel;

			me::components::AabbCollider ac{};
			if (me::GetComponent(e, ac)) {
				b.hasAabb = true;
				b.aabb = ac;
			}

			me::components::CircleCollider cc{};
			if (me::GetComponent(e, cc)) {
				b.hasCircle = true;
				b.circle = cc;
			}

			// Prefer explicit mass from Velocity2D if provided
			if (vel.mass > 0.0f) {
				float m = vel.mass;
				// Optional: clamp to keep sanity
				if (m < 0.1f)  m = 0.1f;
				if (m > 1e6f)  m = 1e6f;

				b.massInfo.mass = m;
				b.massInfo.invMass = 1.0f / m;
			} else {
				// Fallback: infer mass from collider size (old behavior)
				b.massInfo = ComputeMass(b.hasAabb, b.aabb, b.hasCircle, b.circle);
			}

			bodies.push_back(b);
			});

		struct StaticAabb {
			me::EntityId e;
			me::components::Transform2D t;
			me::components::AabbCollider c;
		};
		struct StaticCircle {
			me::EntityId e;
			me::components::Transform2D t;
			me::components::CircleCollider c;
		};

		std::vector<StaticAabb> staticsAabb;
		std::vector<StaticCircle> staticsCircle;
		staticsAabb.reserve(128);
		staticsCircle.reserve(128);

		auto isDynamic = [&](me::EntityId eid) {
			return std::any_of(
				bodies.begin(), bodies.end(),
				[&](const Body& b) { return b.e == eid; }
			);
			};

		// Static AABBs = colliders that are not dynamic
		me::detail::ForEachCollider([&](me::EntityId e, const me::components::AabbCollider& c) {
			if (isDynamic(e)) return;
			me::components::Transform2D t{};
			if (!me::GetComponent(e, t)) return;
			staticsAabb.push_back(StaticAabb{ e, t, c });
			});

		// Static circles = CircleCollider without Velocity2D
		me::detail::ForEachCircleCollider([&](me::EntityId e, const me::components::CircleCollider& c) {
			if (isDynamic(e)) return;
			me::components::Transform2D t{};
			if (!me::GetComponent(e, t)) return;
			staticsCircle.push_back(StaticCircle{ e, t, c });
			});

		// 1) Integrate velocities (+ gravity)
		for (auto& b : bodies) {
			b.v.vx += g_gx * dt;
			b.v.vy += g_gy * dt;
			b.t.x += b.v.vx * dt;
			b.t.y += b.v.vy * dt;
		}

		// 2) Resolve against static colliders
		for (auto& b : bodies) {
			if (b.hasAabb && b.aabb.solid) {
				AABB a = MakeAabb(b.t, b.aabb);

				// AABB vs static AABBs
				for (const auto& s : staticsAabb) {
					if (!s.c.solid) continue;
					AABB sb = MakeAabb(s.t, s.c);
					float pushX = 0, pushY = 0;
					if (OverlapAabbAabb(a, sb, pushX, pushY)) {
						b.t.x += pushX;
						b.t.y += pushY;

						// kill velocity along contact axis (simple but OK vs static)
						if (pushX != 0.0f) b.v.vx = 0.0f;
						if (pushY != 0.0f) b.v.vy = 0.0f;

						a = MakeAabb(b.t, b.aabb);
					}
				}

				// AABB vs static Circles
				for (const auto& s : staticsCircle) {
					if (!s.c.solid) continue;
					CircleShape sc = MakeCircle(s.t, s.c);
					float pushX = 0, pushY = 0;
					if (OverlapCircleAabb(sc, a, pushX, pushY)) {
						// OverlapCircleAabb pushes circle out of box by (pushX, pushY).
						// Here circle is static, AABB is dynamic -> move the AABB opposite.
						b.t.x -= pushX;
						b.t.y -= pushY;

						if (std::fabs(pushX) > std::fabs(pushY))
							b.v.vx = 0.0f;
						else
							b.v.vy = 0.0f;

						a = MakeAabb(b.t, b.aabb);
					}
				}
			} else if (b.hasCircle && b.circle.solid) {
				CircleShape c = MakeCircle(b.t, b.circle);

				// Circle vs static AABBs
				for (const auto& s : staticsAabb) {
					if (!s.c.solid) continue;
					AABB sb = MakeAabb(s.t, s.c);
					float pushX = 0, pushY = 0;
					if (OverlapCircleAabb(c, sb, pushX, pushY)) {
						b.t.x += pushX;
						b.t.y += pushY;

						if (std::fabs(pushX) > std::fabs(pushY))
							b.v.vx = 0.0f;
						else
							b.v.vy = 0.0f;

						c = MakeCircle(b.t, b.circle);
					}
				}

				// Circle vs static circles
				for (const auto& s : staticsCircle) {
					if (!s.c.solid) continue;
					CircleShape sc = MakeCircle(s.t, s.c);
					float pushX = 0, pushY = 0;
					if (OverlapCircleCircle(c, sc, pushX, pushY)) {
						b.t.x += pushX;
						b.t.y += pushY;

						if (std::fabs(pushX) > std::fabs(pushY))
							b.v.vx = 0.0f;
						else
							b.v.vy = 0.0f;

						c = MakeCircle(b.t, b.circle);
					}
				}
			}
		}

		// 3) Dynamic–dynamic collisions with mass & impulses
		const std::size_t n = bodies.size();
		for (std::size_t i = 0; i < n; ++i) {
			for (std::size_t j = i + 1; j < n; ++j) {
				Body& A = bodies[i];
				Body& B = bodies[j];

				bool solidA = (A.hasAabb && A.aabb.solid) || (A.hasCircle && A.circle.solid);
				bool solidB = (B.hasAabb && B.aabb.solid) || (B.hasCircle && B.circle.solid);

				// If either is non-solid, skip physical resolution.
				// (You can still later add some "trigger contact" reporting if you want.)
				if (!solidA || !solidB) {
					continue;
				}

				float dx = 0.0f, dy = 0.0f;
				bool collided = false;

				// Circle–Circle
				if (A.hasCircle && B.hasCircle) {
					CircleShape ca = MakeCircle(A.t, A.circle);
					CircleShape cb = MakeCircle(B.t, B.circle);
					collided = OverlapCircleCircle(ca, cb, dx, dy);
				}
				// Circle–AABB
				else if (A.hasCircle && B.hasAabb) {
					CircleShape ca = MakeCircle(A.t, A.circle);
					AABB       bb = MakeAabb(B.t, B.aabb);
					collided = OverlapCircleAabb(ca, bb, dx, dy);
				} else if (A.hasAabb && B.hasCircle) {
					CircleShape cb = MakeCircle(B.t, B.circle);
					AABB       aa = MakeAabb(A.t, A.aabb);
					collided = OverlapCircleAabb(cb, aa, dx, dy);
					// OverlapCircleAabb(cb, aa, dx, dy) returns "push B (circle) out of A (AABB)".
					// For consistency, flip it so that (dx,dy) is "push A out of B" like the other cases.
					if (collided) {
						dx = -dx;
						dy = -dy;
					}
				}
				// AABB–AABB
				else if (A.hasAabb && B.hasAabb) {
					AABB aa = MakeAabb(A.t, A.aabb);
					AABB bb = MakeAabb(B.t, B.aabb);
					collided = OverlapAabbAabb(aa, bb, dx, dy);
				}

				if (!collided) continue;

				// Compute normal and penetration depth from the correction vector.
				float penLen2 = dx * dx + dy * dy;
				if (penLen2 <= 0.0f) continue;
				float penLen = std::sqrt(penLen2);
				float nx = dx / penLen;
				float ny = dy / penLen;

				float invA = A.massInfo.invMass;
				float invB = B.massInfo.invMass;
				float invSum = invA + invB;
				if (invSum <= 0.0f) invSum = 1.0f;

				// --- Positional correction (mass-weighted) ---
				float moveA = (invA / invSum) * penLen;
				float moveB = (invB / invSum) * penLen;

				A.t.x += nx * moveA;
				A.t.y += ny * moveA;
				B.t.x -= nx * moveB;
				B.t.y -= ny * moveB;

				// --- Velocity impulse along the collision normal ---
				float relVx = B.v.vx - A.v.vx;
				float relVy = B.v.vy - A.v.vy;
				float relAlongN = relVx * nx + relVy * ny;

				// With nx,ny pointing from B -> A (because dx,dy is "push A out of B"),
				// they are approaching if relAlongN > 0.
				// If they're separating or stationary along the normal, don't apply impulse.
				if (relAlongN <= 0.0f) continue;

				float restitution = 0.0f; // 0 = full inelastic, tweak later for bounciness
				float jj = -(1.0f + restitution) * relAlongN / invSum;

				float impulseX = jj * nx;
				float impulseY = jj * ny;

				A.v.vx -= impulseX * invA;
				A.v.vy -= impulseY * invA;
				B.v.vx += impulseX * invB;
				B.v.vy += impulseY * invB;
			}
		}

		// 4) World bounds clamp
		if (g_boundsEnabled) {
			for (auto& b : bodies) {
				if (b.hasAabb) {
					AABB a = MakeAabb(b.t, b.aabb);
					float nx = b.t.x, ny = b.t.y;

					if (a.x < g_minX) { nx += (g_minX - a.x);             b.v.vx = 0.0f; }
					if (a.y < g_minY) { ny += (g_minY - a.y);             b.v.vy = 0.0f; }
					if (a.x + a.w > g_maxX) { nx -= (a.x + a.w - g_maxX);       b.v.vx = 0.0f; }
					if (a.y + a.h > g_maxY) { ny -= (a.y + a.h - g_maxY);       b.v.vy = 0.0f; }

					b.t.x = nx; b.t.y = ny;
				} else if (b.hasCircle) {
					CircleShape c = MakeCircle(b.t, b.circle);
					float nx = b.t.x, ny = b.t.y;

					float left = c.cx - c.r;
					float right = c.cx + c.r;
					float top = c.cy - c.r;
					float bottom = c.cy + c.r;

					if (left < g_minX) {
						float diff = g_minX - left;
						nx += diff;
						b.v.vx = 0.0f;
					}
					if (top < g_minY) {
						float diff = g_minY - top;
						ny += diff;
						b.v.vy = 0.0f;
					}
					if (right > g_maxX) {
						float diff = right - g_maxX;
						nx -= diff;
						b.v.vx = 0.0f;
					}
					if (bottom > g_maxY) {
						float diff = bottom - g_maxY;
						ny -= diff;
						b.v.vy = 0.0f;
					}

					b.t.x = nx; b.t.y = ny;
				}
			}
		}

		// 5) Write back transforms & velocities
		for (auto& b : bodies) {
			me::AddComponent(b.e, b.t);
			me::AddComponent(b.e, b.v);
		}
	}

	void StepFixed(float dt, float fixedStep, float& acc) {
		if (fixedStep <= 0.0f) { Step(dt); return; }
		const int maxSteps = 5;
		acc += dt;
		int steps = 0;
		while (acc >= fixedStep && steps < maxSteps) {
			Step(fixedStep);
			acc -= fixedStep;
			++steps;
		}
		if (steps == maxSteps) acc = 0.0f;
	}
}
