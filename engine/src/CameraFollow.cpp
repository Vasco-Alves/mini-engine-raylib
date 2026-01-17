#include "Entity.hpp"
#include "Components.hpp"
#include "CameraFollow.hpp"

#include <raylib.h>

#include <unordered_map>
#include <utility>
#include <cmath>

namespace {

	struct FollowRec {
		me::EntityId target = 0;
		me::camera::FollowParams params{};
		bool initialized = false;
	};

	// camera entity -> follow record
	std::unordered_map<me::EntityId, FollowRec> s_Following;

	// exponential smoothing factor helper: 1 - exp(-k*dt)
	inline float smoothFactor(float stiffness, float dt) {
		if (stiffness <= 0.0f) return 1.0f; // instant
		return 1.0f - std::exp(-stiffness * dt);
	}
}

namespace me::camera {

	void Attach(me::EntityId camera, me::EntityId target, const FollowParams& p) {
		if (camera == 0 || target == 0) return;
		s_Following[camera] = FollowRec{ target, p, false };
	}

	void Detach(me::EntityId camera) {
		s_Following.erase(camera);
	}

	void Update(float dt) {
		if (s_Following.empty()) return;

		for (auto it = s_Following.begin(); it != s_Following.end(); ) {
			const me::EntityId camE = it->first;
			FollowRec& rec = it->second;

			// fetch camera component
			me::components::Camera2D cam{};
			if (!me::GetComponent(camE, cam)) {
				// camera entity lost its component -> stop following
				it = s_Following.erase(it);
				continue;
			}

			// fetch target transform
			me::components::Transform2D tt{};
			if (!me::GetComponent(rec.target, tt)) {
				// target gone -> stop following
				it = s_Following.erase(it);
				continue;
			}

			const float desiredX = tt.x + rec.params.offsetX;
			const float desiredY = tt.y + rec.params.offsetY;

			if (rec.params.snapOnAttach && !rec.initialized) {
				cam.x = desiredX;
				cam.y = desiredY;
				rec.initialized = true;
			} else {
				const float a = smoothFactor(rec.params.stiffness, dt); // [0..1]
				cam.x = cam.x + (desiredX - cam.x) * a;
				cam.y = cam.y + (desiredY - cam.y) * a;
			}

			// write back camera component
			me::AddComponent(camE, cam);

			++it;
		}
	}

} // namespace me::camera
