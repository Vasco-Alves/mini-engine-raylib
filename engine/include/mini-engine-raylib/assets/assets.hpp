#pragma once

#include "mini-engine-raylib/input/input.hpp"
#include "mini-engine-raylib/core/math.hpp"

#include <cstdint>
#include <string>

namespace me::assets {

	struct TextureId { std::uint32_t handle = 0; };

	// Optional: change where relative URIs resolve from (default: "assets/")
	void set_asset_root(const char* folder);

	// Load (or ref-count) a texture by URI. Example: "player.png"
	TextureId load_texture(const char* uri);

	// Decrement ref-count; unload when it hits zero
	void release(TextureId id);
	void release_all();
	void release_unused();

	bool is_texture_valid(TextureId id);

	// Query texture size in pixels (0,0 if invalid)
	me::math::Vec2 texture_size(TextureId id);
}