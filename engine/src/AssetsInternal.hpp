#pragma once

#include <Assets.hpp>

#include "raylib.h"

namespace me::assets {

	// Internal-only: let Render2D access the loaded Texture2D
	const ::Texture2D* ME_InternalGetTexture(TextureId id);

	// Returns the original URI/key used to load this texture, or nullptr if unknown.
	const char* ME_InternalGetTexturePath(me::assets::TextureId id);

} // namespace me::assets
