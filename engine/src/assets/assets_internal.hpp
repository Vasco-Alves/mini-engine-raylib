#pragma once

#pragma once
#include "mini-engine-raylib/assets/assets.hpp"
#include <raylib.h>

// Forward Declaration
namespace me::raytracing {
	class TriangleBVH;
}

namespace me::assets {

	// --- Internal Texture Access ---
	const ::Texture2D* internal_get_texture(TextureId id);
	const char* internal_get_texture_path(TextureId id);

	// --- Internal Model Access ---
	const ::Model* internal_get_model(ModelId id);
	const char* internal_get_model_path(ModelId id);
	const me::raytracing::TriangleBVH* internal_get_model_bvh(ModelId id);

} // namespace me::assets