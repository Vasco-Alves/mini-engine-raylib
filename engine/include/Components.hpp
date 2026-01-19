#pragma once

#include "Assets.hpp"
#include "Color.hpp"
#include "Audio.hpp" 

namespace me { using EntityId = std::uint32_t; }

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
		float zoom = 1.0f;
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

	struct SpriteSheet {
		me::assets::TextureId tex{};
		int frameW = 0;
		int frameH = 0;
		int startIndex = 0;
		int frameCount = 0;
		int margin = 0;
		int spacing = 0;
		int cols = 0;
	};

	struct AnimationPlayer {
		int   current = 0;
		float fps = 12.0f;
		bool  loop = true;
		bool  playing = true;
		float timeAccum = 0.0f;
	};

	struct CameraFollow {
		me::EntityId target = 0;
		float stiffness = 5.0f;
		float deadzone = 0.0f;
		float offsetX = 0.0f;
		float offsetY = 0.0f;
	};

	struct Lifetime {
		float remaining = 1.0f;
	};

	struct Projectile {
		int damage = 10;
		me::EntityId owner = 0;
	};

	struct Hittable {};

	struct Health {
		int current = 100;
		int max = 100;
	};

} // namespace me::components
