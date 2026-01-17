#include "Animation.hpp"
#include "Entity.hpp"
#include"Registry.hpp"

#include <raylib.h>


namespace me::anim {

	void Update(float dt) {
		if (dt <= 0.0f) return;                 // ignore non-positive dt
		if (dt > 0.1f) dt = 0.1f;               // clamp huge spikes (~10 FPS)

		auto& reg = me::detail::Reg();

		// Iterate all entities with AnimationPlayer; require a SpriteSheet to animate
		for (auto& [e, ap] : reg.animPlayers) {
			if (!ap.playing) continue;

			// must have a sheet
			auto itS = reg.animSheets.find(e);
			if (itS == reg.animSheets.end()) continue;
			const auto& ss = itS->second;

			// basic validation of sheet + player
			if (ss.frameCount <= 0 || ss.frameW <= 0 || ss.frameH <= 0) continue;
			if (ap.fps <= 0.0f) continue;       // <-- fps is on the player

			// keep current in-range (useful after scene loads or clip changes)
			const int start = ss.startIndex;
			const int end = ss.startIndex + ss.frameCount - 1;
			if (ap.current < start || ap.current > end) ap.current = start;

			// advance time
			ap.timeAccum += dt;
			const float frameDur = 1.0f / ap.fps;

			// advance frames (handles large dt by stepping multiple frames)
			while (ap.timeAccum >= frameDur) {
				ap.timeAccum -= frameDur;
				++ap.current;

				if (ap.current > end) {
					if (ap.loop) ap.current = start;
					else { ap.current = end; ap.playing = false; break; }
				}
			}
		}
	}

} // namespace me::anim
