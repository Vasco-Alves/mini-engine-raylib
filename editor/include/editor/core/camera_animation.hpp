#pragma once

// Animation system, Stage 1: a single camera + render-settings track.
//
// A keyframe is a full snapshot (camera pose, fov, exposure, depth of field)
// at a point in time. Frames BETWEEN keyframes are interpolated — keyframes
// are authored anchors, not discrete steps — so two keyframes are enough for
// a smooth camera move, a focus pull, or a brightness ramp.
//
// Stage 2 (later) generalizes this to per-entity component tracks.

#include <raylib.h>
#include <nlohmann/json.hpp>
#include <mini-engine-raylib/ecs/components.hpp>
#include <mini-engine-raylib/systems/raytracer_system.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace editor {

	struct CameraKeyframe {
		float time = 0.0f;              // seconds on the timeline
		Vector3 position{ 0.0f, 5.0f, 10.0f };
		Vector3 target{ 0.0f, 0.0f, 0.0f };
		float fov = 45.0f;
		// Render settings that animate well (brightness ramps, focus pulls).
		float exposure = 0.6f;
		float aperture = 0.0f;
		float focus_distance = 10.0f;
		int easing = 1;                 // 0 = linear, 1 = smooth (ease in/out)
	};

	// Pushes an evaluated keyframe onto the live camera + raytracer settings.
	// (Shared by scrubbing, preview playback and the offline animation render.)
	inline void apply_keyframe(const CameraKeyframe& k,
		me::components::TransformComponent& cam_transform,
		me::components::CameraComponent& camera,
		me::systems::RaytracerSystem& raytracer) {
		cam_transform.position = k.position;
		camera.target = k.target;
		camera.fov = k.fov;
		raytracer.exposure = k.exposure;
		raytracer.aperture = k.aperture;
		raytracer.focus_distance = k.focus_distance;
	}

	class CameraAnimation {
	public:
		std::vector<CameraKeyframe> keys;

		bool  empty()    const { return keys.empty(); }
		float duration() const { return keys.empty() ? 0.0f : keys.back().time; }

		void sort_keys() {
			std::stable_sort(keys.begin(), keys.end(),
				[](const CameraKeyframe& a, const CameraKeyframe& b) { return a.time < b.time; });
		}

		// Blended state at time t, clamped to the track's ends.
		CameraKeyframe evaluate(float t) const {
			if (keys.empty()) return {};
			if (keys.size() == 1 || t <= keys.front().time) return keys.front();
			if (t >= keys.back().time) return keys.back();

			size_t i = 1;
			while (i < keys.size() && keys[i].time < t) ++i;
			const CameraKeyframe& a = keys[i - 1];
			const CameraKeyframe& b = keys[i];

			float span = b.time - a.time;
			float u = (span > 1e-6f) ? (t - a.time) / span : 1.0f;
			if (b.easing == 1) u = u * u * (3.0f - 2.0f * u); // smoothstep into b

			auto lerp3 = [](const Vector3& x, const Vector3& y, float s) {
				return Vector3{ x.x + (y.x - x.x) * s, x.y + (y.y - x.y) * s, x.z + (y.z - x.z) * s };
				};

			CameraKeyframe out;
			out.time = t;
			out.position = lerp3(a.position, b.position, u);
			out.target = lerp3(a.target, b.target, u);
			out.fov = std::lerp(a.fov, b.fov, u);
			out.exposure = std::lerp(a.exposure, b.exposure, u);
			out.aperture = std::lerp(a.aperture, b.aperture, u);
			out.focus_distance = std::lerp(a.focus_distance, b.focus_distance, u);
			out.easing = b.easing;
			return out;
		}

		nlohmann::json to_json() const {
			nlohmann::json arr = nlohmann::json::array();
			for (const auto& k : keys) {
				arr.push_back({
					{"time", k.time},
					{"px", k.position.x}, {"py", k.position.y}, {"pz", k.position.z},
					{"tx", k.target.x},   {"ty", k.target.y},   {"tz", k.target.z},
					{"fov", k.fov},
					{"exposure", k.exposure},
					{"aperture", k.aperture},
					{"focus_distance", k.focus_distance},
					{"easing", k.easing}
					});
			}
			return nlohmann::json{ {"camera_track", arr} };
		}

		void from_json(const nlohmann::json& j) {
			keys.clear();
			if (!j.contains("camera_track")) return;
			for (const auto& e : j["camera_track"]) {
				CameraKeyframe k;
				k.time = e.value("time", 0.0f);
				k.position = { e.value("px", 0.0f), e.value("py", 5.0f), e.value("pz", 10.0f) };
				k.target = { e.value("tx", 0.0f), e.value("ty", 0.0f), e.value("tz", 0.0f) };
				k.fov = e.value("fov", 45.0f);
				k.exposure = e.value("exposure", 0.6f);
				k.aperture = e.value("aperture", 0.0f);
				k.focus_distance = e.value("focus_distance", 10.0f);
				k.easing = e.value("easing", 1);
				keys.push_back(k);
			}
			sort_keys();
		}
	};

} // namespace editor
