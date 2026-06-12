// Headless unit tests for the animation evaluator: keyframe interpolation,
// easing, clamping, JSON lerping and the environment track. Pure math — no
// window, no GPU, no audio device.

#include "editor/core/scene_animation.hpp"

#include <cmath>
#include <cstdio>

namespace {

	int s_failures = 0;

	void check(bool ok, const char* what) {
		if (!ok) {
			printf("[animation_eval] FAILED: %s\n", what);
			s_failures++;
		}
	}

	bool near(float a, float b, float eps = 1e-4f) { return std::abs(a - b) < eps; }

	editor::CameraKeyframe cam_key(float t, float x, float fov, int easing) {
		editor::CameraKeyframe k;
		k.time = t;
		k.position = { x, 0.0f, 0.0f };
		k.fov = fov;
		k.easing = easing;
		return k;
	}

	void test_camera_track() {
		editor::SceneAnimation anim;
		anim.keys.push_back(cam_key(0.0f, 0.0f, 40.0f, 0));   // linear into nothing
		anim.keys.push_back(cam_key(2.0f, 10.0f, 60.0f, 0));  // linear into this key
		anim.sort_keys();

		check(near(anim.duration(), 2.0f), "camera duration is the last key time");

		auto mid = anim.evaluate(1.0f);
		check(near(mid.position.x, 5.0f), "linear midpoint position");
		check(near(mid.fov, 50.0f), "linear midpoint fov");

		auto quarter = anim.evaluate(0.5f);
		check(near(quarter.position.x, 2.5f), "linear quarter position");

		check(near(anim.evaluate(-5.0f).position.x, 0.0f), "clamped before the first key");
		check(near(anim.evaluate(99.0f).position.x, 10.0f), "clamped after the last key");

		// Smooth easing: identical at the midpoint, slower near the ends.
		anim.keys[1].easing = 1;
		check(near(anim.evaluate(1.0f).position.x, 5.0f), "smoothstep(0.5) == 0.5");
		float s = anim.evaluate(0.5f).position.x; // smoothstep(0.25) = 0.15625
		check(near(s, 1.5625f), "smoothstep eases in (slower start)");

		editor::SceneAnimation single;
		single.keys.push_back(cam_key(1.0f, 7.0f, 45.0f, 0));
		check(near(single.evaluate(0.0f).position.x, 7.0f), "single key holds everywhere (before)");
		check(near(single.evaluate(5.0f).position.x, 7.0f), "single key holds everywhere (after)");
	}

	void test_lerp_json() {
		using nlohmann::json;

		json a = { {"v", 0.0}, {"i", 100}, {"s", "hello"}, {"nested", { {"x", 2.0} }} };
		json b = { {"v", 10.0}, {"i", 200}, {"s", "world"}, {"nested", { {"x", 4.0} }} };

		json mid = editor::lerp_json(a, b, 0.5f);
		check(near((float)mid["v"].get<double>(), 5.0f), "numbers lerp");
		check(near((float)mid["i"].get<double>(), 150.0f), "integers lerp as numbers");
		check(near((float)mid["nested"]["x"].get<double>(), 3.0f), "nested objects recurse");

		check(editor::lerp_json(a, b, 0.25f)["s"] == "hello", "strings hold the earlier key (u < 0.5)");
		check(editor::lerp_json(a, b, 0.75f)["s"] == "world", "strings switch to the later key (u >= 0.5)");

		// A key missing from `b` keeps a's value untouched.
		json a2 = { {"only_a", 1.0}, {"both", 0.0} };
		json b2 = { {"both", 2.0} };
		json m2 = editor::lerp_json(a2, b2, 0.5f);
		check(near((float)m2["only_a"].get<double>(), 1.0f), "keys missing in b keep a's value");
		check(near((float)m2["both"].get<double>(), 1.0f), "shared keys lerp");
	}

	void test_entity_track() {
		editor::EntityTrack tr;
		tr.component = "Material";

		editor::EntityKey k0;
		k0.time = 0.0f;
		k0.easing = 0;
		k0.value = { {"emission_power", 0.0}, {"roughness", 1.0} };
		editor::EntityKey k1;
		k1.time = 4.0f;
		k1.easing = 0;
		k1.value = { {"emission_power", 8.0}, {"roughness", 0.0} };
		tr.keys.push_back(k0);
		tr.keys.push_back(k1);
		tr.sort_keys();

		auto mid = tr.evaluate(2.0f);
		check(near((float)mid["emission_power"].get<double>(), 4.0f), "entity snapshot lerps emission");
		check(near((float)mid["roughness"].get<double>(), 0.5f), "entity snapshot lerps roughness");
		check(near((float)tr.evaluate(99.0f)["emission_power"].get<double>(), 8.0f), "entity track clamps at the end");
	}

	void test_environment_track() {
		editor::SceneAnimation anim;

		editor::EnvironmentKeyframe e0;
		e0.time = 0.0f;
		e0.easing = 0;
		e0.sky_intensity = 1.0f;
		e0.sky_zenith = { 0.5f, 0.7f, 1.0f };
		editor::EnvironmentKeyframe e1;
		e1.time = 2.0f;
		e1.easing = 0;
		e1.sky_intensity = 0.0f;
		e1.sky_zenith = { 1.0f, 0.3f, 0.0f };
		anim.env_keys.push_back(e0);
		anim.env_keys.push_back(e1);
		anim.sort_env_keys();

		auto mid = anim.evaluate_env(1.0f);
		check(near(mid.sky_intensity, 0.5f), "environment intensity lerps");
		check(near(mid.sky_zenith.x, 0.75f), "environment colors lerp");

		check(near(anim.duration(), 2.0f), "duration includes the environment track");
		check(anim.is_renderable(), "two environment keys are renderable");
	}

} // namespace

int main() {
	printf("[animation_eval] running...\n");

	test_camera_track();
	test_lerp_json();
	test_entity_track();
	test_environment_track();

	if (s_failures == 0) {
		printf("[animation_eval] PASSED\n");
		return 0;
	}
	printf("[animation_eval] %d check(s) FAILED\n", s_failures);
	return 1;
}
