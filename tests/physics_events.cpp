// Headless collision-event + raycast tests — no window, GPU or audio device
// (Jolt runs entirely on the CPU). Builds a small world on a local Registry:
//
//   floor    Static box at the origin
//   ball     Dynamic sphere dropped onto the floor
//   zone     Static box flagged is_trigger, 50 m away
//   drop     Dynamic sphere dropped through the trigger
//
// and verifies that contact enter/exit events fire (deduplicated per pair),
// that triggers report overlaps without colliding, that raycasts hit the right
// bodies, and that removing a body mid-play emits the exit event.

#include <mini-ecs/registry.hpp>
#include <mini-engine-raylib/systems/physics_system.hpp>
#include <mini-engine-raylib/ecs/components.hpp>
#include <mini-engine-raylib/ecs/physics_components.hpp>

#include <cmath>
#include <iostream>
#include <vector>

using namespace me::components;

static int g_failures = 0;

#define CHECK(cond)                                                            \
	do {                                                                       \
		if (!(cond)) {                                                         \
			std::cerr << "  FAIL: " << #cond << "  (line " << __LINE__ << ")\n"; \
			++g_failures;                                                      \
		}                                                                      \
	} while (0)

static me::Entity make_body(me::Registry& reg, const char* name, Vector3 pos,
	RigidBodyType type, bool trigger = false) {
	auto e = reg.create_entity();
	e.add_component(TagComponent{ name });
	TransformComponent t;
	t.position = pos;
	e.add_component(t);
	RigidBodyComponent rb;
	rb.type = type;
	rb.bounciness = 0.0f; // settle instead of bouncing — keeps the event stream clean
	rb.is_trigger = trigger;
	e.add_component(rb);
	return e;
}

// Steps the simulation and appends every contact event produced.
static void step(me::Registry& reg, int frames, std::vector<me::physics::ContactEvent>& out) {
	for (int i = 0; i < frames; ++i) {
		me::physics::update(reg, 1.0f / 60.0f);
		for (const auto& ev : me::physics::consume_contact_events()) out.push_back(ev);
	}
}

static bool has_event(const std::vector<me::physics::ContactEvent>& events,
	me::entity::entity_id a, me::entity::entity_id b, bool entered) {
	for (const auto& ev : events) {
		if (ev.entered != entered) continue;
		if ((ev.a == a && ev.b == b) || (ev.a == b && ev.b == a)) return true;
	}
	return false;
}

// enters minus exits for a pair: +1 while touching, 0 once fully separated.
// (A settling body can legitimately chatter enter/exit/enter — only the net
// balance is deterministic.)
static int net_contacts(const std::vector<me::physics::ContactEvent>& events,
	me::entity::entity_id a, me::entity::entity_id b) {
	int net = 0;
	for (const auto& ev : events) {
		if ((ev.a == a && ev.b == b) || (ev.a == b && ev.b == a))
			net += ev.entered ? 1 : -1;
	}
	return net;
}

int main() {
	std::cout << "[physics_events] running...\n";

	me::physics::init();
	me::Registry reg;

	auto floor = make_body(reg, "Floor", { 0, 0, 0 }, RigidBodyType::Static);
	floor.add_component(BoxColliderComponent{ { 10.0f, 0.5f, 10.0f } });

	auto ball = make_body(reg, "Ball", { 0, 3, 0 }, RigidBodyType::Dynamic);
	ball.add_component(SphereColliderComponent{ 0.5f });

	auto zone = make_body(reg, "Zone", { 50, 0, 0 }, RigidBodyType::Static, /*trigger*/ true);
	zone.add_component(BoxColliderComponent{ { 2.0f, 0.5f, 2.0f } });

	auto drop = make_body(reg, "Drop", { 50, 3, 0 }, RigidBodyType::Dynamic);
	drop.add_component(SphereColliderComponent{ 0.5f });

	me::physics::on_play(reg);

	// --- fall, land, overlap: 3 simulated seconds ---
	std::vector<me::physics::ContactEvent> events;
	step(reg, 180, events);

	// the ball hit the floor and is resting on it now (net one live contact)
	CHECK(has_event(events, ball.get_id(), floor.get_id(), true));
	CHECK(net_contacts(events, ball.get_id(), floor.get_id()) == 1);

	// the trigger zone reported the overlap...
	CHECK(has_event(events, drop.get_id(), zone.get_id(), true));

	// ...but didn't physically block: the drop fell straight through
	auto* drop_t = reg.try_get_component<TransformComponent>(drop.get_id());
	CHECK(drop_t && drop_t->position.y < -1.0f);

	// while the solid floor really did catch the ball (rest height = 0.5 + 0.5)
	auto* ball_t = reg.try_get_component<TransformComponent>(ball.get_id());
	CHECK(ball_t && std::fabs(ball_t->position.y - 1.0f) < 0.2f);

	// --- raycasts ---
	auto hit = me::physics::raycast({ 5, 5, 0 }, { 0, -1, 0 }, 100.0f); // clear floor area
	CHECK(hit.hit);
	CHECK(hit.entity == floor.get_id());
	CHECK(std::fabs(hit.distance - 4.5f) < 0.05f); // floor top is at y = 0.5
	CHECK(hit.normal.y > 0.9f);

	hit = me::physics::raycast({ 0, 5, 0 }, { 0, -1, 0 }, 100.0f);      // straight at the ball
	CHECK(hit.hit && hit.entity == ball.get_id());

	hit = me::physics::raycast({ 0, 5, 100 }, { 0, -1, 0 }, 100.0f);    // empty space
	CHECK(!hit.hit);

	hit = me::physics::raycast({ 5, 5, 0 }, { 0, -1, 0 }, 2.0f);        // too short to reach
	CHECK(!hit.hit);

	// --- removing a body mid-play emits the exit for its live contacts ---
	// (covers the sleeping case: a settled body's removal isn't reported by
	// Jolt, the engine flushes the pair itself)
	me::physics::remove_body(reg, ball.get_id());
	for (const auto& ev : me::physics::consume_contact_events()) events.push_back(ev);
	step(reg, 2, events);
	CHECK(has_event(events, ball.get_id(), floor.get_id(), false));
	CHECK(net_contacts(events, ball.get_id(), floor.get_id()) == 0);

	// and the ray that hit the ball now passes through to the floor
	hit = me::physics::raycast({ 0, 5, 0 }, { 0, -1, 0 }, 100.0f);
	CHECK(hit.hit && hit.entity == floor.get_id());

	me::physics::on_stop();

	// outside play mode everything degrades to inert results
	CHECK(me::physics::consume_contact_events().empty());
	CHECK(!me::physics::raycast({ 0, 5, 0 }, { 0, -1, 0 }, 100.0f).hit);

	me::physics::shutdown();

	if (g_failures == 0) {
		std::cout << "[physics_events] PASSED\n";
		return 0;
	}
	std::cerr << "[physics_events] FAILED (" << g_failures << " check(s))\n";
	return 1;
}
