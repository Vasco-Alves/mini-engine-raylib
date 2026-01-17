#pragma once

#include <string>

#include "Entity.hpp"
#include "Components.hpp"
#include "Color.hpp"


namespace me::dbg {

	// Simple screen-space text for HUD
	void Text(float x, float y, const std::string& text, int size = 20);

	void  Text(float x, float y, const std::string& text, const Color& color, int size = 20);

	// World-space helpers (called inside BeginCamera / EndCamera)
	void DrawAabbWorld(me::EntityId e, const me::components::Transform2D& t, const me::components::AabbCollider& c);

	void DrawCircleWorld(me::EntityId e, const me::components::Transform2D& t, const me::components::CircleCollider& c);

	// Draw all colliders in the world (for debugging)
	void DrawAllCollidersWorld();

} // namespace me::dbg
