#include "DebugDraw.hpp"
#include "Components.hpp"
#include "Entity.hpp"
#include "Registry.hpp"

#include <raylib.h>
#include <string>

namespace me::dbg {

	// ------------------------------------------------------------
	// Screen-space debug text
	// ------------------------------------------------------------
	void Text(float x, float y, const std::string& text, int size) {
		DrawText(text.c_str(), (int)x, (int)y, size, ::BLACK);
	}

	void Text(float x, float y, const std::string& text, const me::Color& color, int size) {
		::Color c = { color.r, color.g, color.b, color.a };
		DrawText(text.c_str(), (int)x, (int)y, size, c);
	}

	// ------------------------------------------------------------
	// Helpers
	// ------------------------------------------------------------

	static void DrawAabbWorld(me::EntityId,
		const me::components::Transform2D& t,
		const me::components::AabbCollider& c) {

		// Collider center in world space
		float cx = t.x + c.ox;
		float cy = t.y + c.oy;

		// Convert to top-left for drawing
		float x = cx - c.w * 0.5f;
		float y = cy - c.h * 0.5f;

		DrawRectangleLines((int)x, (int)y, (int)c.w, (int)c.h, ::RED);
	}

	static void DrawCircleWorld(me::EntityId,
		const me::components::Transform2D& t,
		const me::components::CircleCollider& c) {

		float cx = t.x + c.ox;
		float cy = t.y + c.oy;

		DrawCircleLines((int)cx, (int)cy, c.radius, ::GREEN);
	}

	// ------------------------------------------------------------
	// Main Draw Loop
	// ------------------------------------------------------------
	void DrawAllCollidersWorld() {
		auto& reg = me::detail::Reg();

		// 1. Iterate AABB Pool
		if (auto* pool = reg.TryGetPool<me::components::AabbCollider>()) {
			for (const auto& kv : pool->data) {
				me::EntityId e = kv.first;
				const auto& collider = kv.second;

				// We need a transform to draw it
				if (auto* t = reg.TryGetComponent<me::components::Transform2D>(e)) {
					DrawAabbWorld(e, *t, collider);
				}
			}
		}

		// 2. Iterate Circle Pool
		if (auto* pool = reg.TryGetPool<me::components::CircleCollider>()) {
			for (const auto& kv : pool->data) {
				me::EntityId e = kv.first;
				const auto& collider = kv.second;

				if (auto* t = reg.TryGetComponent<me::components::Transform2D>(e)) {
					DrawCircleWorld(e, *t, collider);
				}
			}
		}
	}

} // namespace me::dbg