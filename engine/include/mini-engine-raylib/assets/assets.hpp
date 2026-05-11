#pragma once

#include <cstdint>
#include <string>

#include <raylib.h>

namespace me::assets {

	struct TextureId { std::uint32_t handle = 0; };

	struct ModelId { std::uint32_t handle = 0; };

	// Optional: change where relative URIs resolve from (default: "assets/")
	void set_asset_root(const char* folder);

	// Load (or ref-count) a texture by URI. Example: "player.png"
	TextureId load_texture(const char* uri);

	// Decrement ref-count; unload when it hits zero
	void release(TextureId id);
	void release_all();
	//void release_unused();

	bool is_texture_valid(TextureId id);

	// Query texture size in pixels (0,0 if invalid)
	Vector2 texture_size(TextureId id);

	// --- 3D Models ---
	ModelId load_model(const char* uri);
	void release(ModelId id);
	bool is_model_valid(ModelId id);

} // namespace me::assets