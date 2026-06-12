// Headless round-trip test for scene serialization — no window or audio device.
//
// Builds a scene with one entity carrying (almost) every persisted component plus
// a parent/child hierarchy, then verifies that:
//   * save -> load -> save produces identical JSON (serialize/deserialize is stable), and
//   * key values and the parent/child links survive the round trip.
//
// Asset-backed components (Model/AudioSource/BackgroundMusic) are included with
// empty paths so their load path never touches the GPU/audio device.

#include <mini-ecs/registry.hpp>
#include <mini-engine-raylib/scene/scene_manager.hpp>
#include <mini-engine-raylib/ecs/components.hpp>
#include <mini-engine-raylib/ecs/physics_components.hpp>
#include <mini-engine-raylib/ecs/audio_components.hpp>
#include <mini-engine-raylib/ecs/script_component.hpp>

#include <nlohmann/json.hpp>

#include <cmath>
#include <fstream>
#include <iostream>
#include <string>

using namespace me::components;

static int g_failures = 0;

#define CHECK(cond)                                                            \
	do {                                                                       \
		if (!(cond)) {                                                         \
			std::cerr << "  FAIL: " << #cond << "  (line " << __LINE__ << ")\n"; \
			++g_failures;                                                      \
		}                                                                      \
	} while (0)

static nlohmann::ordered_json read_json(const std::string& path) {
	std::ifstream f(path);
	nlohmann::ordered_json j;
	f >> j;
	return j;
}

static me::entity::entity_id find_by_tag(me::Registry& r, const std::string& name) {
	auto& pool = r.view<TagComponent>();
	for (size_t i = 0; i < pool.size(); ++i)
		if (pool.components[i].name == name) return pool.entity_map[i];
	return me::entity::null;
}

int main() {
	const std::string path_a = "rt_scene_a.json";
	const std::string path_b = "rt_scene_b.json";

	std::cout << "[scene_roundtrip] running...\n";

	// --- Build a source scene: one fully-loaded entity + a child ---
	me::Registry reg;

	auto parent = reg.create_entity();
	parent.add_component(TagComponent{ "Parent" });
	{
		TransformComponent t;
		t.position = { 1.5f, -2.0f, 3.25f };
		t.rotation = { 10.f, 20.f, 30.f };
		t.scale = { 2.f, 3.f, 4.f };
		parent.add_component(t);
	}
	parent.add_component(Shape3DComponent{ Shape3DComponent::Sphere, me::Color{ 10, 20, 30, 255 }, true });
	parent.add_component(MaterialComponent{ me::Color{ 200, 100, 50, 128 }, 0.3f, 0.8f, 1.25f, 0.4f, 1.7f });
	parent.add_component(Model3DComponent{});       // empty handle -> no asset load
	parent.add_component(LightComponent{ me::Color{ 255, 128, 0, 255 }, 2.5f });
	parent.add_component(DirectionalLightComponent{ me::Color{ 100, 110, 120, 255 }, 0.75f });
	parent.add_component(CameraComponent{});
	parent.add_component(RigidBodyComponent{ RigidBodyType::Dynamic, 3.0f, 0.5f, 0.7f });
	parent.add_component(BoxColliderComponent{ { 1.5f, 2.5f, 3.5f }, false });
	parent.add_component(SphereColliderComponent{ 2.25f, false });
	parent.add_component(AudioListenerComponent{ true });
	{
		AudioSourceComponent a;     // empty filepath -> no device load
		a.volume = 0.42f; a.pitch = 1.3f; a.spatial = false; a.max_distance = 12.0f;
		parent.add_component(a);
	}
	{
		BackgroundMusicComponent b; // empty filepath -> no device load
		b.volume = 0.6f; b.loop = false;
		parent.add_component(b);
	}
	{
		ScriptComponent sc;
		sc.scripts.push_back({ "scripts/test.lua" });
		parent.add_component(sc);
	}

	auto child = reg.create_entity();
	child.add_component(TagComponent{ "Child" });
	{
		TransformComponent t;
		t.position = { 5.f, 0.f, 0.f };
		t.parent = parent.get_id();
		child.add_component(t);
	}
	parent.get_component<TransformComponent>().children.push_back(child.get_id());

	// --- save -> load -> save, then diff ---
	CHECK(me::scene_manager::save(reg, path_a));

	me::Registry reg2;
	CHECK(me::scene_manager::load(reg2, path_a));
	CHECK(me::scene_manager::save(reg2, path_b));

	nlohmann::ordered_json a = read_json(path_a);
	nlohmann::ordered_json b = read_json(path_b);
	CHECK(a == b);                       // serialize/deserialize is stable
	CHECK(a["entities"].size() == 2);

	// --- value + hierarchy spot checks after load ---
	auto p = find_by_tag(reg2, "Parent");
	auto c = find_by_tag(reg2, "Child");
	CHECK(p != me::entity::null);
	CHECK(c != me::entity::null);

	auto* pt = reg2.try_get_component<TransformComponent>(p);
	auto* ct = reg2.try_get_component<TransformComponent>(c);
	CHECK(pt && std::fabs(pt->position.x - 1.5f) < 1e-4f);
	CHECK(pt && std::fabs(pt->scale.z - 4.0f) < 1e-4f);
	CHECK(ct && std::fabs(ct->position.x - 5.0f) < 1e-4f);

	// hierarchy survived the id remap
	CHECK(ct && ct->parent == p);
	CHECK(pt && pt->children.size() == 1 && pt->children[0] == c);

	// a few component values survived
	auto* mat = reg2.try_get_component<MaterialComponent>(p);
	CHECK(mat && mat->albedo.a == 128);
	CHECK(mat && std::fabs(mat->metallic - 0.8f) < 1e-4f);

	auto* rb = reg2.try_get_component<RigidBodyComponent>(p);
	CHECK(rb && rb->type == RigidBodyType::Dynamic && std::fabs(rb->mass - 3.0f) < 1e-4f);

	auto* scr = reg2.try_get_component<ScriptComponent>(p);
	CHECK(scr && scr->scripts.size() == 1 && scr->scripts[0].path == "scripts/test.lua");

	if (g_failures == 0) {
		std::cout << "[scene_roundtrip] PASSED\n";
		return 0;
	}
	std::cerr << "[scene_roundtrip] FAILED (" << g_failures << " check(s))\n";
	return 1;
}
