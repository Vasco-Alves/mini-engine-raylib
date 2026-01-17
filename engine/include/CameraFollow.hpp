#pragma once
#include <cstdint>
#include "Entity.hpp"
#include "Components.hpp"

namespace me::camera {

	struct FollowParams {
		float offsetX = 0.0f;  // offset from target (world units)
		float offsetY = 0.0f;
		float stiffness = 8.0f; // higher = snappier follow (exp-smoothing)
		bool  snapOnAttach = true; // place camera on target immediately at attach
	};

	// Attach camera entity to follow target entity
	void Attach(me::EntityId camera, me::EntityId target, const FollowParams& p = {});

	// Stop following (camera keeps last position)
	void Detach(me::EntityId camera);

	// Update all following cameras (call once per frame before RenderWorld)
	void Update(float dt);
}
