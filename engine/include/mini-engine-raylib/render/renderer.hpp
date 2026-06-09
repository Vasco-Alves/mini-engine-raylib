#pragma once

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

	// --- Render ---
	// If passed overrides, it renders from that perspective. If null, it automatically searches the ECS
	void render_world(const me::components::TransformComponent* override_transform = nullptr, const me::components::CameraComponent* override_cam = nullptr);

} // namespace me::render