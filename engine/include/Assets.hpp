#pragma once

#include <cstdint>
#include <string>

#include "Input.hpp"
#include "Math.hpp"

namespace me::assets {

	struct TextureId { std::uint32_t handle = 0; };

	// Optional: change where relative URIs resolve from (default: "assets/")
	void SetAssetRoot(const char* folder);

	// Load (or ref-count) a texture by URI. Example: "player.png"
	TextureId LoadTexture(const char* uri);

	// Decrement ref-count; unload when it hits zero
	void Release(TextureId id);

	void ReleaseAll();

	void ReleaseUnused();

	bool IsTextureValid(TextureId id);

	// Query texture size in pixels (0,0 if invalid)
	me::math::Vec2 TextureSize(TextureId id);
}
