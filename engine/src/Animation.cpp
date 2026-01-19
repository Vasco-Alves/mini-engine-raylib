#include "Animation.hpp"
#include "Components.hpp"
#include "Registry.hpp"

namespace me::systems {

	void Animation_Update(float dt) {
		auto& reg = me::detail::Reg();
		auto* pool = reg.GetPool<me::components::AnimationPlayer>();

		for (auto& kv : pool->data) {
			me::EntityId e = kv.first;
			auto& anim = kv.second;

			if (!anim.playing) continue;

			// Need a sheet to know frame count
			auto* sheet = reg.TryGetComponent<me::components::SpriteSheet>(e);
			if (!sheet || sheet->frameCount <= 0) continue;

			anim.timeAccum += dt;
			float frameDur = 1.0f / (anim.fps > 0 ? anim.fps : 1.0f);

			if (anim.timeAccum >= frameDur) {
				anim.timeAccum -= frameDur;
				anim.current++;

				if (anim.current >= sheet->frameCount) {
					if (anim.loop) {
						anim.current = 0;
					} else {
						anim.current = sheet->frameCount - 1;
						anim.playing = false;
					}
				}
			}
		}
	}

} // namespace me::anim