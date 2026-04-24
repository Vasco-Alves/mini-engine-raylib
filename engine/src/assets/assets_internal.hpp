#pragma once

#include "mini-engine-raylib/assets/assets.hpp"

#include <raylib.h>

namespace me::assets {

	// Internal-only: let Render2D access the loaded Texture2D
	const ::Texture2D* internal_get_texture(TextureId id);

	// Returns the original URI/key used to load this texture, or nullptr if unknown.
	const char* internal_get_texture_path(TextureId id);

} // namespace me::assets