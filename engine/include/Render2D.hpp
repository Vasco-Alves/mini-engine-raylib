#pragma once

#include <cstdint>

#include "Entity.hpp"   // for EntityId
#include "Assets.hpp"
#include "Color.hpp"    // shared color type

namespace me::render2d {

	struct SpriteDesc {
		me::assets::TextureId tex{};
		float x = 0.0f, y = 0.0f;
		float rotation = 0.0f;
		float scaleX = 1.0f, scaleY = 1.0f;
		float originX = 0.0f, originY = 0.0f;
		me::Color tint{};
		bool flipX = false, flipY = false;
	};

	// Draw a single sprite directly to the current render target.
	void DrawSprite(const SpriteDesc& s);

	// Set which camera entity is considered "active" for world rendering.
	void SetActiveCamera(me::EntityId e);

	// Begin rendering in world space using the active camera (if any).
	// If no active camera, this is a no-op.
	void BeginCamera();

	// End world-space rendering started by BeginCamera().
	void EndCamera();

	// Draw all entities with SpriteRenderer (+Transform2D) in world space.
	// Assumes you've already called BeginCamera() if you want camera transform.
	void RenderWorld();

	// Render the world through a specific camera into a screen-space viewport.
	// (x, y) = top-left in pixels. (w, h) = size in pixels.
	void RenderWorldWithCamera(me::EntityId camera, int x, int y, int w, int h);

	void ClearWorld(const me::Color& clearColor);
}
