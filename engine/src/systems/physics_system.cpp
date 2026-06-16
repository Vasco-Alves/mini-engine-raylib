#include "mini-engine-raylib/systems/physics_system.hpp"

#include <mini-ecs/registry.hpp>
#include "mini-engine-raylib/core/engine.hpp"
#include "mini-engine-raylib/core/logger.hpp"
#include "mini-engine-raylib/ecs/components.hpp"
#include "mini-engine-raylib/ecs/physics_components.hpp"

#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <unordered_map>
#include <rlgl.h>

// --- JOLT HEADERS ---
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/AllowedDOFs.h>

JPH_SUPPRESS_WARNINGS

namespace me::physics {

	// ==========================================
	// JOLT BOILERPLATE & CONFIGURATION
	// ==========================================

	static void TraceImpl(const char* inFMT, ...) {
		va_list list;
		va_start(list, inFMT);
		char buffer[1024];
		vsnprintf(buffer, sizeof(buffer), inFMT, list);
		va_end(list);
		me::logger::info("[JOLT] " + std::string(buffer));
	}

#ifdef JPH_ENABLE_ASSERTS
	static bool AssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile, uint32_t inLine) {
		me::logger::error("[JOLT ASSERT] " + std::string(inFile) + ":" + std::to_string(inLine) + " " + inExpression + " " + (inMessage ? inMessage : ""));
		return false;
	}
#endif

	namespace Layers {
		static constexpr JPH::ObjectLayer NON_MOVING = 0;
		static constexpr JPH::ObjectLayer MOVING = 1;
		static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
	};

	namespace BroadPhaseLayers {
		static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
		static constexpr JPH::BroadPhaseLayer MOVING(1);
		static constexpr uint32_t NUM_LAYERS(2);
	};

	class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
	public:
		BPLayerInterfaceImpl() {
			mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
			mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
		}
		virtual uint32_t GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYERS; }
		virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override { return mObjectToBroadPhase[inLayer]; }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
		virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
			return (inLayer == BroadPhaseLayers::NON_MOVING) ? "NON_MOVING" : "MOVING";
		}
#endif
	private:
		JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
	};

	class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter {
	public:
		virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override {
			switch (inLayer1) {
			case Layers::NON_MOVING: return inLayer2 == BroadPhaseLayers::MOVING;
			case Layers::MOVING: return true;
			default: return false;
			}
		}
	};

	class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter {
	public:
		virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override {
			switch (inObject1) {
			case Layers::NON_MOVING: return inObject2 == Layers::MOVING;
			case Layers::MOVING: return true;
			default: return false;
			}
		}
	};

	// ==========================================
	// CONTACT EVENT COLLECTOR
	// ==========================================
	// Jolt calls the contact listener from its worker threads while Update() runs,
	// so everything here is mutex-guarded and only *records*; nothing user-facing
	// (Lua, the registry) is touched until consume() on the main thread.
	//
	// Enter/exit is tracked per body PAIR with a touch count: a pair can gain and
	// lose several sub-shape contacts, but scripts should see exactly one
	// on_collision_enter when the first lands and one on_collision_exit when the
	// last separates. Entity ids travel in the bodies' user data.
	class ContactEventCollector final : public JPH::ContactListener {
	public:
		void OnContactAdded(const JPH::Body& body1, const JPH::Body& body2,
			const JPH::ContactManifold&, JPH::ContactSettings&) override {
			std::lock_guard<std::mutex> lock(m_Mutex);
			PairRecord& rec = m_Pairs[pair_key(body1.GetID(), body2.GetID())];
			if (rec.touch_count++ == 0) {
				rec.a = static_cast<me::entity::entity_id>(body1.GetUserData());
				rec.b = static_cast<me::entity::entity_id>(body2.GetUserData());
				m_Events.push_back({ rec.a, rec.b, true });
			}
		}

		void OnContactRemoved(const JPH::SubShapeIDPair& pair) override {
			std::lock_guard<std::mutex> lock(m_Mutex);
			auto it = m_Pairs.find(pair_key(pair.GetBody1ID(), pair.GetBody2ID()));
			if (it == m_Pairs.end()) return;
			if (--it->second.touch_count <= 0) {
				m_Events.push_back({ it->second.a, it->second.b, false });
				m_Pairs.erase(it);
			}
		}

		std::vector<ContactEvent> consume() {
			std::lock_guard<std::mutex> lock(m_Mutex);
			std::vector<ContactEvent> out;
			out.swap(m_Events);
			return out;
		}

		// Emits the exit events for every live pair involving one body and forgets
		// those pairs. Used when a body is removed mid-play: Jolt won't report
		// contact removal for a body that was asleep when it vanished, so the
		// engine settles the books itself. If Jolt does report the removal on the
		// next step (awake case), the pair is already gone here and the callback
		// finds nothing — no double event.
		void flush_body(JPH::BodyID id) {
			std::lock_guard<std::mutex> lock(m_Mutex);
			const std::uint32_t needle = id.GetIndexAndSequenceNumber();
			for (auto it = m_Pairs.begin(); it != m_Pairs.end(); ) {
				const std::uint32_t hi = static_cast<std::uint32_t>(it->first >> 32);
				const std::uint32_t lo = static_cast<std::uint32_t>(it->first);
				if (hi == needle || lo == needle) {
					m_Events.push_back({ it->second.a, it->second.b, false });
					it = m_Pairs.erase(it);
				} else {
					++it;
				}
			}
		}

	private:
		// One key per unordered body pair, so (a,b) and (b,a) match.
		static std::uint64_t pair_key(JPH::BodyID a, JPH::BodyID b) {
			std::uint32_t x = a.GetIndexAndSequenceNumber();
			std::uint32_t y = b.GetIndexAndSequenceNumber();
			if (x > y) std::swap(x, y);
			return (static_cast<std::uint64_t>(x) << 32) | y;
		}

		struct PairRecord {
			me::entity::entity_id a = me::entity::null;
			me::entity::entity_id b = me::entity::null;
			int touch_count = 0;
		};

		std::mutex m_Mutex;
		std::unordered_map<std::uint64_t, PairRecord> m_Pairs;
		std::vector<ContactEvent> m_Events;
	};

	static JPH::PhysicsSystem* s_PhysicsSystem = nullptr;
	static JPH::TempAllocatorImpl* s_TempAllocator = nullptr;
	static JPH::JobSystemThreadPool* s_JobSystem = nullptr;
	static BPLayerInterfaceImpl* s_BPLayerInterface = nullptr;
	static ObjectVsBroadPhaseLayerFilterImpl* s_ObjectVsBroadphaseFilter = nullptr;
	static ObjectLayerPairFilterImpl* s_ObjectVsObjectFilter = nullptr;
	static ContactEventCollector* s_ContactCollector = nullptr;

	// ==========================================
	// ENGINE LIFECYCLE
	// ==========================================

	void init() {
		JPH::RegisterDefaultAllocator();
		JPH::Trace = TraceImpl;
		JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = AssertFailedImpl;)

			JPH::Factory::sInstance = new JPH::Factory();
		JPH::RegisterTypes();

		me::logger::info("Jolt Physics Core Initialized");
	}

	void shutdown() {
		JPH::UnregisterTypes();
		delete JPH::Factory::sInstance;
		JPH::Factory::sInstance = nullptr;
	}

	// ==========================================
	// BODY CREATION (shared by on_play and add_body)
	// ==========================================

	// Creates and registers the Jolt body for one entity. Requires a Transform and a
	// collider next to the RigidBody; quietly skips the entity otherwise.
	static bool create_body(me::Registry& registry, me::entity::entity_id e, me::components::RigidBodyComponent& rb) {
		auto* transform = registry.try_get_component<me::components::TransformComponent>(e);

		// Extract collider types
		auto* box_col = registry.try_get_component<me::components::BoxColliderComponent>(e);
		auto* sphere_col = registry.try_get_component<me::components::SphereColliderComponent>(e);

		if (!transform || (!box_col && !sphere_col)) return false;

		JPH::BodyInterface& body_interface = s_PhysicsSystem->GetBodyInterface();
		JPH::ShapeRefC shape;

		// --- BOX COLLIDER MATH ---
		if (box_col) {
			float ext_x = std::abs(box_col->half_extents.x * transform->scale.x);
			float ext_y = std::abs(box_col->half_extents.y * transform->scale.y);
			float ext_z = std::abs(box_col->half_extents.z * transform->scale.z);

			if (ext_x <= 0.001f || ext_y <= 0.001f || ext_z <= 0.001f) {
				me::logger::warn("Skipping Box Entity " + std::to_string(e) + " - Physics Colliders cannot have a thickness of zero!");
				return false;
			}

			JPH::BoxShapeSettings shape_settings(JPH::Vec3(ext_x, ext_y, ext_z));
			JPH::ShapeSettings::ShapeResult shape_result = shape_settings.Create();

			if (!shape_result.IsValid()) {
				me::logger::error("Jolt failed to create box shape for Entity " + std::to_string(e));
				return false;
			}
			shape = shape_result.Get();
		}

		// --- SPHERE COLLIDER MATH ---
		else if (sphere_col) {
			float scaled_radius = std::abs(sphere_col->radius * transform->scale.x);

			if (scaled_radius <= 0.001f) {
				me::logger::warn("Skipping Sphere Entity " + std::to_string(e) + " - Radius cannot be zero!");
				return false;
			}

			JPH::SphereShapeSettings shape_settings(scaled_radius);
			JPH::ShapeSettings::ShapeResult shape_result = shape_settings.Create();

			if (!shape_result.IsValid()) {
				me::logger::error("Jolt failed to create sphere shape for Entity " + std::to_string(e));
				return false;
			}
			shape = shape_result.Get();
		}

		// --- COMMON BODY CREATION ---
		Quaternion ray_quat = QuaternionFromEuler(
			transform->rotation.x * DEG2RAD,
			transform->rotation.y * DEG2RAD,
			transform->rotation.z * DEG2RAD
		);

		JPH::RVec3 position(transform->position.x, transform->position.y, transform->position.z);
		JPH::Quat rotation(ray_quat.x, ray_quat.y, ray_quat.z, ray_quat.w);

		JPH::ObjectLayer layer = (rb.type == me::components::RigidBodyType::Static) ? Layers::NON_MOVING : Layers::MOVING;
		JPH::EMotionType motion_type = JPH::EMotionType::Dynamic;
		if (rb.type == me::components::RigidBodyType::Static) motion_type = JPH::EMotionType::Static;
		if (rb.type == me::components::RigidBodyType::Kinematic) motion_type = JPH::EMotionType::Kinematic;

		JPH::BodyCreationSettings body_settings(shape, position, rotation, motion_type, layer);
		body_settings.mRestitution = rb.bounciness;
		body_settings.mFriction = rb.friction;
		body_settings.mIsSensor = rb.is_trigger;

		// Lock requested rotation axes (e.g. an upright character that must not tip
		// over). Start from all six DOFs and clear the frozen rotation bits.
		uint8_t dofs = static_cast<uint8_t>(JPH::EAllowedDOFs::All);
		if (rb.freeze_rot_x) dofs &= ~static_cast<uint8_t>(JPH::EAllowedDOFs::RotationX);
		if (rb.freeze_rot_y) dofs &= ~static_cast<uint8_t>(JPH::EAllowedDOFs::RotationY);
		if (rb.freeze_rot_z) dofs &= ~static_cast<uint8_t>(JPH::EAllowedDOFs::RotationZ);
		body_settings.mAllowedDOFs = static_cast<JPH::EAllowedDOFs>(dofs);
		// The contact listener and raycasts read the entity id back out of here.
		body_settings.mUserData = static_cast<JPH::uint64>(e);
		// Jolt reports OnContactRemoved when an island falls asleep (~0.5 s of
		// rest), which would fire a false on_collision_exit on every resting
		// body — a grounded player would "leave the floor" by standing still.
		// At this engine's scene scale, keeping bodies awake is the simple,
		// correct trade: contact events always mirror physical reality.
		body_settings.mAllowSleeping = false;
		if (motion_type == JPH::EMotionType::Dynamic) {
			body_settings.mMassPropertiesOverride.mMass = std::max(rb.mass, 0.001f);
			body_settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
		}

		JPH::Body* body = body_interface.CreateBody(body_settings);
		if (body == nullptr) {
			me::logger::error("Jolt returned a null body for Entity " + std::to_string(e));
			return false;
		}

		rb.runtime_body_id = body->GetID().GetIndexAndSequenceNumber();
		body_interface.AddBody(body->GetID(), JPH::EActivation::Activate);
		return true;
	}

	// ==========================================
	// PLAY / STOP LIFECYCLE
	// ==========================================

	void on_play(me::Registry& registry) {
		s_TempAllocator = new JPH::TempAllocatorImpl(10 * 1024 * 1024);
		s_JobSystem = new JPH::JobSystemThreadPool(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, -1);

		s_BPLayerInterface = new BPLayerInterfaceImpl();
		s_ObjectVsBroadphaseFilter = new ObjectVsBroadPhaseLayerFilterImpl();
		s_ObjectVsObjectFilter = new ObjectLayerPairFilterImpl();

		s_PhysicsSystem = new JPH::PhysicsSystem();
		s_PhysicsSystem->Init(1024, 0, 1024, 1024, *s_BPLayerInterface, *s_ObjectVsBroadphaseFilter, *s_ObjectVsObjectFilter);

		s_ContactCollector = new ContactEventCollector();
		s_PhysicsSystem->SetContactListener(s_ContactCollector);

		for (auto [e, rb] : registry.view<me::components::RigidBodyComponent>())
			create_body(registry, e, rb);

		s_PhysicsSystem->OptimizeBroadPhase();
	}

	void add_body(me::Registry& registry, me::entity::entity_id e) {
		if (!s_PhysicsSystem) return; // not simulating — the next on_play picks the entity up

		auto* rb = registry.try_get_component<me::components::RigidBodyComponent>(e);
		if (!rb || rb->runtime_body_id != 0xFFFFFFFF) return; // no RigidBody, or already live

		create_body(registry, e, *rb);
	}

	void remove_body(me::Registry& registry, me::entity::entity_id e) {
		if (!s_PhysicsSystem) return; // on_stop destroys the whole physics world anyway

		auto* rb = registry.try_get_component<me::components::RigidBodyComponent>(e);
		if (!rb || rb->runtime_body_id == 0xFFFFFFFF) return;

		JPH::BodyInterface& body_interface = s_PhysicsSystem->GetBodyInterface();
		JPH::BodyID id(rb->runtime_body_id);
		if (body_interface.IsAdded(id)) body_interface.RemoveBody(id);
		body_interface.DestroyBody(id);
		rb->runtime_body_id = 0xFFFFFFFF;

		// The body's live contacts end now — scripts get their on_collision_exit.
		if (s_ContactCollector) s_ContactCollector->flush_body(id);
	}

	void on_stop() {
		// Clean up the physics simulation entirely
		delete s_PhysicsSystem; s_PhysicsSystem = nullptr;
		delete s_ContactCollector; s_ContactCollector = nullptr;
		delete s_ObjectVsObjectFilter; s_ObjectVsObjectFilter = nullptr;
		delete s_ObjectVsBroadphaseFilter; s_ObjectVsBroadphaseFilter = nullptr;
		delete s_BPLayerInterface; s_BPLayerInterface = nullptr;
		delete s_JobSystem; s_JobSystem = nullptr;
		delete s_TempAllocator; s_TempAllocator = nullptr;
	}

	void update(me::Registry& registry, float dt) {
		if (!s_PhysicsSystem) return;

		// Skip physics step if dt is zero
		if (dt <= 0.0f) return;

		// A long stall (window drag, debugger break) must not make the simulation
		// try to catch up with one giant step — clamp the frame to a sane maximum.
		dt = std::min(dt, 0.1f);

		// Jolt wants individual steps no longer than ~1/60 s; it divides dt by the
		// collision-step count internally, so subdividing big frames keeps slow
		// machines from tunneling fast bodies through thin colliders.
		const int collision_steps = std::clamp(static_cast<int>(std::ceil(dt * 60.0f)), 1, 6);
		s_PhysicsSystem->Update(dt, collision_steps, s_TempAllocator, s_JobSystem);

		JPH::BodyInterface& body_interface = s_PhysicsSystem->GetBodyInterface();

		for (auto [e, rb, transform] : registry.view<me::components::RigidBodyComponent, me::components::TransformComponent>()) {
			(void)e;
			if (rb.type != me::components::RigidBodyType::Static && rb.runtime_body_id != 0xFFFFFFFF) {
				JPH::BodyID id(rb.runtime_body_id);

				if (!body_interface.IsAdded(id)) continue;

				JPH::RVec3 pos = body_interface.GetPosition(id);
				JPH::Quat rot = body_interface.GetRotation(id);

				transform.position = { pos.GetX(), pos.GetY(), pos.GetZ() };

				Quaternion ray_quat = { rot.GetX(), rot.GetY(), rot.GetZ(), rot.GetW() };
				Vector3 euler = QuaternionToEuler(ray_quat);
				transform.rotation = { euler.x * RAD2DEG, euler.y * RAD2DEG, euler.z * RAD2DEG };

				transform.is_dirty = true;
			}
		}
	}

	void draw_debug(me::Registry& registry) {
		for (auto [e, rb, transform] : registry.view<me::components::RigidBodyComponent, me::components::TransformComponent>()) {
			auto* box_col = registry.try_get_component<me::components::BoxColliderComponent>(e);
			auto* sphere_col = registry.try_get_component<me::components::SphereColliderComponent>(e);

			if (box_col || sphere_col) {
				::Color wire_color = { 0, 228, 48, 255 }; // Green (Dynamic)

				if (rb.type == me::components::RigidBodyType::Static)
					wire_color = { 230, 41, 55, 255 }; // Red (Static)

				if (rb.type == me::components::RigidBodyType::Kinematic)
					wire_color = { 0, 121, 241, 255 }; // Blue (Kinematic)

				rlPushMatrix();

				// Inject entity's transform matrix directly into the GPU
				Matrix glMatrix = MatrixTranspose(transform.model_matrix);
				rlMultMatrixf((float*)&glMatrix);

				if (box_col && box_col->show_debug) {
					// BoxColliders use Half-Extents. Raylib draws using Full-Extents (Width, Height, Length).
					float w = box_col->half_extents.x * 2.0f;
					float h = box_col->half_extents.y * 2.0f;
					float l = box_col->half_extents.z * 2.0f;

					DrawCubeWires({ 0.0f, 0.0f, 0.0f }, w, h, l, wire_color);
				} else if (sphere_col && sphere_col->show_debug) {
					DrawSphereWires({ 0.0f, 0.0f, 0.0f }, sphere_col->radius, 16, 16, wire_color);
				}

				rlPopMatrix();
			}
		}
	}

	void set_linear_velocity(me::entity::entity_id e, float x, float y, float z) {
		if (!s_PhysicsSystem) return;

		auto& reg = me::get_registry();
		auto* rb = reg.try_get_component<me::components::RigidBodyComponent>(e);

		// Make sure the entity actually has a rigid body that was spawned by Jolt
		if (rb && rb->runtime_body_id != 0xFFFFFFFF) {
			JPH::BodyInterface& body_interface = s_PhysicsSystem->GetBodyInterface();
			JPH::BodyID id(rb->runtime_body_id);

			// Ensure the body still exists in the physics world
			if (body_interface.IsAdded(id)) {
				body_interface.ActivateBody(id);
				body_interface.SetLinearVelocity(id, JPH::Vec3(x, y, z));
			}
		}
	}

	void set_local_linear_velocity(me::entity::entity_id e, float x, float y, float z) {
		if (!s_PhysicsSystem) return;

		auto& reg = me::get_registry();
		auto* rb = reg.try_get_component<me::components::RigidBodyComponent>(e);
		auto* t = reg.try_get_component<me::components::TransformComponent>(e);

		if (rb && t && rb->runtime_body_id != 0xFFFFFFFF) {
			JPH::BodyInterface& body_interface = s_PhysicsSystem->GetBodyInterface();
			JPH::BodyID id(rb->runtime_body_id);

			if (body_interface.IsAdded(id)) {
				// Extract the entity's basis vectors from its world matrix (column-major):
				//   Forward = col 2 = (m2, m6, m10)  — local +Z
				//   Right   = col 0 = (m0, m4, m8)   — local +X
				//   Up      = col 1 = (m1, m5, m9)   — local +Y
				// Normalize them to strip out any scale that's baked into the matrix.
				Vector3 right = Vector3Normalize({ t->model_matrix.m0,  t->model_matrix.m4,  t->model_matrix.m8 });
				Vector3 up = Vector3Normalize({ t->model_matrix.m1,  t->model_matrix.m5,  t->model_matrix.m9 });
				Vector3 forward = Vector3Normalize({ t->model_matrix.m2,  t->model_matrix.m6,  t->model_matrix.m10 });

				// Build the world-space velocity by combining the local axes
				Vector3 world_vel = {
					right.x * x + up.x * y + forward.x * z,
					right.y * x + up.y * y + forward.y * z,
					right.z * x + up.z * y + forward.z * z,
				};

				body_interface.ActivateBody(id);
				body_interface.SetLinearVelocity(id, JPH::Vec3(world_vel.x, world_vel.y, world_vel.z));
			}
		}
	}

	// Looks up an entity's live Jolt body; returns an invalid id when the entity has no
	// RigidBody, the body was never created, or the simulation isn't running.
	static JPH::BodyID find_body(me::entity::entity_id e) {
		if (!s_PhysicsSystem) return JPH::BodyID();

		auto& reg = me::get_registry();
		auto* rb = reg.try_get_component<me::components::RigidBodyComponent>(e);
		if (!rb || rb->runtime_body_id == 0xFFFFFFFF) return JPH::BodyID();

		JPH::BodyID id(rb->runtime_body_id);
		if (!s_PhysicsSystem->GetBodyInterface().IsAdded(id)) return JPH::BodyID();
		return id;
	}

	Vector3 get_linear_velocity(me::entity::entity_id e) {
		JPH::BodyID id = find_body(e);
		if (id.IsInvalid()) return { 0.0f, 0.0f, 0.0f };

		JPH::Vec3 v = s_PhysicsSystem->GetBodyInterface().GetLinearVelocity(id);
		return { v.GetX(), v.GetY(), v.GetZ() };
	}

	void set_angular_velocity(me::entity::entity_id e, float x, float y, float z) {
		JPH::BodyID id = find_body(e);
		if (id.IsInvalid()) return;

		JPH::BodyInterface& body_interface = s_PhysicsSystem->GetBodyInterface();
		body_interface.ActivateBody(id);
		body_interface.SetAngularVelocity(id, JPH::Vec3(x, y, z));
	}

	void apply_impulse(me::entity::entity_id e, float x, float y, float z) {
		JPH::BodyID id = find_body(e);
		if (id.IsInvalid()) return;

		// AddImpulse activates the body itself.
		s_PhysicsSystem->GetBodyInterface().AddImpulse(id, JPH::Vec3(x, y, z));
	}

	void teleport_body(me::entity::entity_id e, float x, float y, float z) {
		JPH::BodyID id = find_body(e);
		if (id.IsInvalid()) return;

		// Static bodies must not be activated (Jolt asserts); they can still be moved.
		auto* rb = me::get_registry().try_get_component<me::components::RigidBodyComponent>(e);
		const bool is_static = rb && rb->type == me::components::RigidBodyType::Static;

		s_PhysicsSystem->GetBodyInterface().SetPosition(id, JPH::RVec3(x, y, z),
			is_static ? JPH::EActivation::DontActivate : JPH::EActivation::Activate);
	}

	std::vector<ContactEvent> consume_contact_events() {
		if (!s_ContactCollector) return {};
		return s_ContactCollector->consume();
	}

	RaycastHit raycast(Vector3 origin, Vector3 direction, float max_distance) {
		RaycastHit out{};
		if (!s_PhysicsSystem || max_distance <= 0.0f) return out;
		if (Vector3LengthSqr(direction) < 1e-12f) return out;

		Vector3 d = Vector3Scale(Vector3Normalize(direction), max_distance);
		JPH::RRayCast ray{ JPH::RVec3(origin.x, origin.y, origin.z), JPH::Vec3(d.x, d.y, d.z) };

		JPH::RayCastResult result;
		if (!s_PhysicsSystem->GetNarrowPhaseQuery().CastRay(ray, result)) return out;

		out.hit = true;
		out.distance = result.mFraction * max_distance;
		JPH::RVec3 p = ray.GetPointOnRay(result.mFraction);
		out.point = { p.GetX(), p.GetY(), p.GetZ() };

		// The entity id and the surface normal need the body itself.
		JPH::BodyLockRead lock(s_PhysicsSystem->GetBodyLockInterface(), result.mBodyID);
		if (lock.Succeeded()) {
			const JPH::Body& body = lock.GetBody();
			out.entity = static_cast<me::entity::entity_id>(body.GetUserData());
			JPH::Vec3 n = body.GetWorldSpaceSurfaceNormal(result.mSubShapeID2, p);
			out.normal = { n.GetX(), n.GetY(), n.GetZ() };
		}
		return out;
	}

} // namespace me::physics
