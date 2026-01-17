#include "DebugDraw.hpp"
#include "Components.hpp"
#include "Entity.hpp"
#include "ComponentsInternal.hpp"
#include "Registry.hpp"

#include <raylib.h>

namespace me::dbg {

	// ------------------------------------------------------------
	// Screen-space debug text (no camera)
	// ------------------------------------------------------------
	void Text(float x, float y, const std::string& text, int size) {
		DrawText(text.c_str(), (int)x, (int)y, size, ::BLACK);
	}

	void Text(float x, float y, const std::string& text, const Color& color, int size) {
		::Color c = { color.r, color.g, color.b, color.a };
		DrawText(text.c_str(), (int)x, (int)y, size, c);
	}

	// ------------------------------------------------------------
	// World-space collider drawing (center-based Transform)
	// ------------------------------------------------------------

	// AABB: Transform2D.x/y is center; AabbCollider.ox/oy offset center
	void DrawAabbWorld(me::EntityId,
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

	// Circle: Transform2D.x/y is center; CircleCollider.ox/oy offset center
	void DrawCircleWorld(me::EntityId,
		const me::components::Transform2D& t,
		const me::components::CircleCollider& c) {
		float cx = t.x + c.ox;
		float cy = t.y + c.oy;

		DrawCircleLines((int)cx, (int)cy, c.radius, ::GREEN);
	}

	// ------------------------------------------------------------
	// Draw all colliders in world space
	// ------------------------------------------------------------
	void DrawAllCollidersWorld() {
		// AABBs
		me::detail::ForEachCollider([&](me::EntityId e, const me::components::AabbCollider& c) {
			me::components::Transform2D t{};
			if (!me::GetComponent(e, t)) return;
			DrawAabbWorld(e, t, c);
			});

		// Circles
		me::detail::ForEachCircleCollider([&](me::EntityId e, const me::components::CircleCollider& c) {
			me::components::Transform2D t{};
			if (!me::GetComponent(e, t)) return;
			DrawCircleWorld(e, t, c);
			});
	}

} // namespace me::dbg
