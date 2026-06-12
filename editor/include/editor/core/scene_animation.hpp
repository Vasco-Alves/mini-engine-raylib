#pragma once

// Animation system, Stage 2: a camera track + any number of entity tracks.
//
// A keyframe is a snapshot at a point in time; frames BETWEEN keyframes are
// interpolated, so keyframes are authored anchors, not discrete steps.
//
// Entity tracks animate whole components, captured and re-applied through the
// engine component registry (me::ecs). A key stores the component's scene-JSON
// snapshot; evaluation lerps every numeric field between the surrounding keys
// and loads the blended snapshot back onto the entity. That makes any
// *data-only* component animatable with zero per-component code (asset-backed
// components like Model/Audio would re-acquire handles every frame, so the
// panel restricts tracks to a safe whitelist).
//
// Entities are referenced by id in memory and by Tag name in the sidecar file
// (ids are not stable across scene loads); tracks re-resolve by name on load.

#include <raylib.h>
#include <nlohmann/json.hpp>
#include <mini-ecs/registry.hpp>
#include <mini-engine-raylib/ecs/components.hpp>
#include <mini-engine-raylib/ecs/component_registry.hpp>
#include <mini-engine-raylib/systems/raytracer_system.hpp>
#include <mini-engine-raylib/core/logger.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace editor {

	// =========================================================================
	// CAMERA TRACK
	// =========================================================================
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

	// Pushes an evaluated camera keyframe onto the live camera + raytracer.
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

	// =========================================================================
	// ENVIRONMENT TRACK — the raytracer's sky + ambient settings over time
	// (sunsets, sky ramps, ambient fades). Captured from the live Raytracer
	// Settings values, exactly like camera keys capture the live camera.
	// =========================================================================
	struct EnvironmentKeyframe {
		float time = 0.0f;
		int easing = 1;            // 0 = linear, 1 = smooth (ease in/out)
		float ambient_strength = 0.03f;
		Vector3 sky_horizon{ 1.0f, 1.0f, 1.0f };
		Vector3 sky_zenith{ 0.5f, 0.7f, 1.0f };
		float sky_intensity = 1.0f;
	};

	inline void apply_environment(const EnvironmentKeyframe& k, me::systems::RaytracerSystem& raytracer) {
		raytracer.ambient_strength = k.ambient_strength;
		raytracer.sky_horizon_color = k.sky_horizon;
		raytracer.sky_zenith_color = k.sky_zenith;
		raytracer.sky_intensity = k.sky_intensity;
	}

	// =========================================================================
	// JSON INTERPOLATION — numbers lerp, objects recurse, everything else
	// holds the earlier key's value until the next key takes over.
	// =========================================================================
	inline nlohmann::json lerp_json(const nlohmann::json& a, const nlohmann::json& b, float u) {
		if (a.is_number() && b.is_number()) {
			double x = a.get<double>();
			double y = b.get<double>();
			return x + (y - x) * (double)u;
		}
		if (a.is_object() && b.is_object()) {
			nlohmann::json out = a;
			for (auto& [key, val] : out.items()) {
				if (b.contains(key)) out[key] = lerp_json(val, b[key], u);
			}
			return out;
		}
		return (u < 0.5f) ? a : b;
	}

	// =========================================================================
	// ENTITY TRACK — one animated component on one entity
	// =========================================================================
	struct EntityKey {
		float time = 0.0f;
		int easing = 1;            // 0 = linear, 1 = smooth (ease in/out)
		nlohmann::json value;      // component snapshot (me::ecs save format)
	};

	struct EntityTrack {
		me::entity::entity_id entity = me::entity::null; // live id (session only)
		std::string entity_name;   // Tag name — used by the sidecar + display
		std::string component;     // registry meta name, e.g. "Material"
		std::vector<EntityKey> keys;

		void sort_keys() {
			std::stable_sort(keys.begin(), keys.end(),
				[](const EntityKey& a, const EntityKey& b) { return a.time < b.time; });
		}

		// Blended component snapshot at time t, clamped to the track's ends.
		nlohmann::json evaluate(float t) const {
			if (keys.empty()) return nlohmann::json::object();
			if (keys.size() == 1 || t <= keys.front().time) return keys.front().value;
			if (t >= keys.back().time) return keys.back().value;

			size_t i = 1;
			while (i < keys.size() && keys[i].time < t) ++i;
			const EntityKey& a = keys[i - 1];
			const EntityKey& b = keys[i];

			float span = b.time - a.time;
			float u = (span > 1e-6f) ? (t - a.time) / span : 1.0f;
			if (b.easing == 1) u = u * u * (3.0f - 2.0f * u); // smoothstep into b

			return lerp_json(a.value, b.value, u);
		}
	};

	// =========================================================================
	// SCENE ANIMATION — the camera track plus all entity tracks
	// =========================================================================
	class SceneAnimation {
	public:
		std::vector<CameraKeyframe> keys; // camera track
		std::vector<EnvironmentKeyframe> env_keys; // environment (sky/ambient) track
		std::vector<EntityTrack> entity_tracks;

		bool empty() const { return keys.empty() && env_keys.empty() && entity_tracks.empty(); }

		float duration() const {
			float d = keys.empty() ? 0.0f : keys.back().time;
			if (!env_keys.empty()) d = std::max(d, env_keys.back().time);
			for (const auto& tr : entity_tracks)
				if (!tr.keys.empty()) d = std::max(d, tr.keys.back().time);
			return d;
		}

		// Something actually moves: at least one track with two or more keys.
		bool is_renderable() const {
			if (duration() <= 0.0f) return false;
			if (keys.size() >= 2 || env_keys.size() >= 2) return true;
			for (const auto& tr : entity_tracks)
				if (tr.keys.size() >= 2) return true;
			return false;
		}

		void sort_keys() {
			std::stable_sort(keys.begin(), keys.end(),
				[](const CameraKeyframe& a, const CameraKeyframe& b) { return a.time < b.time; });
		}

		void sort_env_keys() {
			std::stable_sort(env_keys.begin(), env_keys.end(),
				[](const EnvironmentKeyframe& a, const EnvironmentKeyframe& b) { return a.time < b.time; });
		}

		// Blended environment at time t, clamped to the track's ends.
		EnvironmentKeyframe evaluate_env(float t) const {
			if (env_keys.empty()) return {};
			if (env_keys.size() == 1 || t <= env_keys.front().time) return env_keys.front();
			if (t >= env_keys.back().time) return env_keys.back();

			size_t i = 1;
			while (i < env_keys.size() && env_keys[i].time < t) ++i;
			const EnvironmentKeyframe& a = env_keys[i - 1];
			const EnvironmentKeyframe& b = env_keys[i];

			float span = b.time - a.time;
			float u = (span > 1e-6f) ? (t - a.time) / span : 1.0f;
			if (b.easing == 1) u = u * u * (3.0f - 2.0f * u);

			auto lerp3 = [](const Vector3& x, const Vector3& y, float s) {
				return Vector3{ x.x + (y.x - x.x) * s, x.y + (y.y - x.y) * s, x.z + (y.z - x.z) * s };
				};

			EnvironmentKeyframe out;
			out.time = t;
			out.easing = b.easing;
			out.ambient_strength = std::lerp(a.ambient_strength, b.ambient_strength, u);
			out.sky_horizon = lerp3(a.sky_horizon, b.sky_horizon, u);
			out.sky_zenith = lerp3(a.sky_zenith, b.sky_zenith, u);
			out.sky_intensity = std::lerp(a.sky_intensity, b.sky_intensity, u);
			return out;
		}

		// Blended camera state at time t, clamped to the track's ends.
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

		// ------------------------------------------------------------------
		// Sidecar serialization
		// ------------------------------------------------------------------
		nlohmann::json to_json() const {
			nlohmann::json cam = nlohmann::json::array();
			for (const auto& k : keys) {
				cam.push_back({
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

			nlohmann::json env = nlohmann::json::array();
			for (const auto& k : env_keys) {
				env.push_back({
					{"time", k.time}, {"easing", k.easing},
					{"ambient", k.ambient_strength},
					{"hx", k.sky_horizon.x}, {"hy", k.sky_horizon.y}, {"hz", k.sky_horizon.z},
					{"zx", k.sky_zenith.x},  {"zy", k.sky_zenith.y},  {"zz", k.sky_zenith.z},
					{"intensity", k.sky_intensity}
					});
			}

			nlohmann::json tracks = nlohmann::json::array();
			for (const auto& tr : entity_tracks) {
				nlohmann::json track_keys = nlohmann::json::array();
				for (const auto& k : tr.keys) {
					track_keys.push_back({ {"time", k.time}, {"easing", k.easing}, {"value", k.value} });
				}
				tracks.push_back({
					{"entity", tr.entity_name},
					{"component", tr.component},
					{"keys", track_keys}
					});
			}

			return nlohmann::json{ {"camera_track", cam}, {"environment_track", env}, {"entity_tracks", tracks} };
		}

		void from_json(const nlohmann::json& j) {
			keys.clear();
			env_keys.clear();
			entity_tracks.clear();

			if (j.contains("camera_track")) {
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

			if (j.contains("environment_track")) {
				for (const auto& e : j["environment_track"]) {
					EnvironmentKeyframe k;
					k.time = e.value("time", 0.0f);
					k.easing = e.value("easing", 1);
					k.ambient_strength = e.value("ambient", 0.03f);
					k.sky_horizon = { e.value("hx", 1.0f), e.value("hy", 1.0f), e.value("hz", 1.0f) };
					k.sky_zenith = { e.value("zx", 0.5f), e.value("zy", 0.7f), e.value("zz", 1.0f) };
					k.sky_intensity = e.value("intensity", 1.0f);
					env_keys.push_back(k);
				}
				sort_env_keys();
			}

			if (j.contains("entity_tracks")) {
				for (const auto& e : j["entity_tracks"]) {
					EntityTrack tr;
					tr.entity_name = e.value("entity", "");
					tr.component = e.value("component", "");
					if (e.contains("keys")) {
						for (const auto& ke : e["keys"]) {
							EntityKey k;
							k.time = ke.value("time", 0.0f);
							k.easing = ke.value("easing", 1);
							if (ke.contains("value")) k.value = ke["value"];
							tr.keys.push_back(k);
						}
					}
					tr.sort_keys();
					entity_tracks.push_back(std::move(tr));
				}
			}
		}

		// Re-binds tracks to live entities by Tag name (ids change every scene
		// load). Unresolved tracks are kept but inert, with a console warning.
		void resolve_entities(me::Registry& reg) {
			auto& tags = reg.view<me::components::TagComponent>();
			for (auto& tr : entity_tracks) {
				tr.entity = me::entity::null;
				for (size_t i = 0; i < tags.size(); ++i) {
					if (tags.components[i].name == tr.entity_name && reg.is_alive(tags.entity_map[i])) {
						tr.entity = tags.entity_map[i];
						break;
					}
				}
				if (tr.entity == me::entity::null) {
					me::logger::warn("Animation track for '" + tr.entity_name + "' (" + tr.component
						+ ") found no entity with that name — the track is inactive.");
				}
			}
		}
	};

	// Applies every entity track at time t. Transform keeps its live hierarchy
	// links: the registry load replaces the whole component, and Transform's
	// loader only reads the local TRS — parent/children must survive the swap.
	inline void apply_entity_tracks(const SceneAnimation& anim, float t, me::Registry& reg) {
		for (const auto& tr : anim.entity_tracks) {
			if (tr.keys.empty() || tr.entity == me::entity::null || !reg.is_alive(tr.entity)) continue;
			const auto* meta = me::ecs::find(tr.component);
			if (!meta) continue;

			nlohmann::json blended = tr.evaluate(t);

			if (tr.component == "Transform") {
				auto* tc = reg.try_get_component<me::components::TransformComponent>(tr.entity);
				auto parent = tc ? tc->parent : me::entity::null;
				auto children = tc ? tc->children : std::vector<me::entity::entity_id>{};
				meta->load(reg, tr.entity, blended);
				if (auto* nt = reg.try_get_component<me::components::TransformComponent>(tr.entity)) {
					nt->parent = parent;
					nt->children = children;
				}
			} else {
				meta->load(reg, tr.entity, blended);
			}
		}
	}

} // namespace editor
