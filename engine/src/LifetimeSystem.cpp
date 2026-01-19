#include "LifetimeSystem.hpp"
#include "Components.hpp"
#include "Registry.hpp"
#include "Entity.hpp"

namespace me::systems {

	void Lifetime_Update(float dt) {
		auto& reg = me::detail::Reg();

		// Use TryGetPool to avoid creating it if no lifetimes exist yet
		auto* pool = reg.TryGetPool<me::components::Lifetime>();
		if (!pool) return;

		// Iterate safely (since we might destroy entities)
		auto it = pool->data.begin();
		while (it != pool->data.end()) {
			me::EntityId e = it->first;
			auto& life = it->second;

			life.remaining -= dt;

			if (life.remaining <= 0.0f) {
				// DestroyEntity will invalidate the iterator because it erases from the map
				// So we increment BEFORE destroying (and store ID)
				// Actually, erasing from unordered_map invalidates only the erased element.
				// But to be safe and clean:

				me::EntityId deadId = e;
				++it; // Move to next
				me::DestroyEntity(deadId);
			} else {
				++it;
			}
		}
	}
}