#pragma once

#include "mini-engine-raylib/render/color.hpp"

namespace me::components {
	struct TransformComponent;
	struct CameraComponent;
}

namespace me::render {

	void clear_world(me::Color color);

	// If passed overrides, it renders from that perspective. If null, it automatically searches the ECS
	void render_world(const me::components::TransformComponent* override_transform = nullptr, const me::components::CameraComponent* override_cam = nullptr);

	void render_2d();

} // namespace me::render
