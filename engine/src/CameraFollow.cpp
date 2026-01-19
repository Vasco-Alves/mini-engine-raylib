#include "CameraFollow.hpp"
#include "Components.hpp"
#include "Registry.hpp"
#include "Entity.hpp"

#include <cmath>

namespace me::systems {

	static float Lerp(float start, float end, float amount) {
		return start + amount * (end - start);
	}

	void CameraFollow_Update(float dt) {
		auto& reg = me::detail::Reg();

		auto* pool = reg.TryGetPool<me::components::CameraFollow>();
		if (!pool) return;

		for (auto& kv : pool->data) {
			me::EntityId camEntity = kv.first;
			const auto& follow = kv.second;

			auto* camT = reg.TryGetComponent<me::components::Transform2D>(camEntity);
			if (!camT) continue;

			if (!me::IsAlive(follow.target)) continue;

			auto* targetT = reg.TryGetComponent<me::components::Transform2D>(follow.target);
			if (!targetT) continue;

			// Logic
			float targetX = targetT->x + follow.offsetX;
			float targetY = targetT->y + follow.offsetY;

			float t = follow.stiffness * dt;
			if (t > 1.0f) t = 1.0f;

			camT->x = Lerp(camT->x, targetX, t);
			camT->y = Lerp(camT->y, targetY, t);
		}
	}
}