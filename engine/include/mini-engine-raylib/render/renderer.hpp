#pragma once

#include <raylib.h>

#include "mini-engine-raylib/render/color.hpp"

namespace me::components {
	struct TransformComponent;
	struct CameraComponent;
}

namespace me::render {

	// --- Lifecycle ---
	void init();
	void shutdown();

	void clear_world(me::Color color);

	// --- Lighting Toggle ---
	bool is_lighting_enabled();
	void set_lighting_enabled(bool enabled);

	// --- Shadows ---
	// Renders the directional light's shadow map: a depth-only pass over all
	// shapes and models from the sun's point of view, with the ortho volume
	// centered on `focus` (give it the viewer's camera position so shadows
	// follow the player). The pass binds its own framebuffer, so call it once
	// per frame BEFORE binding the final render target (the editor's viewport
	// texture / the game's backbuffer drawing). No-op when there is no
	// directional light, the light has cast_shadows off, or lighting is off —
	// render_world then simply draws unshadowed.
	void render_shadows(Vector3 focus);

	// --- Render ---
	// If passed overrides, it renders from that perspective. If null, it automatically searches the ECS
	void render_world(const me::components::TransformComponent* override_transform = nullptr, const me::components::CameraComponent* override_cam = nullptr);

} // namespace me::render