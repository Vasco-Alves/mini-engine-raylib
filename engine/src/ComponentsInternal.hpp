#pragma once

#include "Components.hpp"
#include "Entity.hpp"

#include <functional>

namespace me::detail {

	// Components.cpp
	void ForEachSprite(const std::function<void(me::EntityId, const me::components::SpriteRenderer&)>& fn);
	void OnEntityDestroyed(me::EntityId e);

	// ComponentsCamera.cpp
	void ForEachCamera(const std::function<void(me::EntityId, const me::components::Camera2D&)>& fn);
	void OnEntityDestroyed_Camera(me::EntityId e);

	// ComponentsPhysics.cpp
	void ForEachVelocity(const std::function<void(me::EntityId, const me::components::Velocity2D&)>& fn);
	void ForEachCollider(const std::function<void(me::EntityId, const me::components::AabbCollider&)>& fn);
	void ForEachCircleCollider(const std::function<void(me::EntityId, const me::components::CircleCollider&)>& fn);
	void OnEntityDestroyed_Physics(me::EntityId e);

	// ComponentsAnimation.cpp
	void ForEachAnimSheet(const std::function<void(me::EntityId, const me::components::SpriteSheet&)>& fn);
	void ForEachAnimPlayer(const std::function<void(me::EntityId, me::components::AnimationPlayer&)>& fn);
	void OnEntityDestroyed_Animation(me::EntityId e);

} // namespace me::detail
