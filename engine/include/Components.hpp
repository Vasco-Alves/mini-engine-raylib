#pragma once

#include <cstdint>

#include "Assets.hpp"
#include "Color.hpp"
#include "Audio.hpp" 

namespace me::components {

	struct Transform2D {
		float x = 0.0f, y = 0.0f;
		float rotation = 0.0f;
		float sx = 1.0f, sy = 1.0f;
	};

	struct SpriteRenderer {
		me::assets::TextureId tex{};
		me::Color             tint{};      // use shared Color
		bool  flipX = false, flipY = false;
		int   layer = 0;
		float originX = 0.0f, originY = 0.0f;
	};

	struct Camera2D {
		float x = 0.0f, y = 0.0f;
		float zoom = 1.0f;
		float rotation = 0.0f;
		bool  active = false;
	};

	struct Velocity2D {
		float vx = 0.0f;
		float vy = 0.0f;

		// If > 0, engine uses this as mass (kg-ish units).
		// If <= 0, engine falls back to auto mass from collider size.
		float mass = 0.0f;
	};

	struct AabbCollider {
		float w = 32.0f;    // width  in world units (pixels)
		float h = 32.0f;    // height in world units (pixels)
		float ox = 0.0f;    // offset from Transform2D.x/y
		float oy = 0.0f;
		bool  solid = true; // if false, passes through (still detectable later if needed)
	};

	struct CircleCollider {
		float radius = 16.0f;  // radius in world units (pixels)
		float ox = 0.0f;       // offset from Transform2D.x/y to center
		float oy = 0.0f;
		bool  solid = true;
	};

	// Flipbook sheet description(static data about the atlas)
	struct SpriteSheet {
		me::assets::TextureId tex{};
		int frameW = 0;
		int frameH = 0;
		int startIndex = 0;
		int frameCount = 0;
		int margin = 0;   // optional padding around the sheet
		int spacing = 0;  // optional spacing between frames
		int cols = 0;     // 0 = auto compute from texture width
	};

	// Runtime animation state (per-entity clock)
	struct AnimationPlayer {
		int   current = 0;     // absolute frame index (sheet-local)
		float fps = 8.0f;
		bool  loop = true;
		bool  playing = true;
		float timeAccum = 0.0f;  // seconds
	};

} // namespace me::components
