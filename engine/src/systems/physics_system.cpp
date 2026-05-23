#include "mini-engine-raylib/systems/physics_system.hpp"

#include <mini-ecs/registry.hpp>
#include "mini-engine-raylib/core/engine.hpp"
#include "mini-engine-raylib/core/logger.hpp"
#include "mini-engine-raylib/ecs/components.hpp"
#include "mini-engine-raylib/ecs/physics_components.hpp"

#include <raymath.h>
#include <cstdarg>
#include <iostream>
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
#include <Jolt/Physics/Body/BodyCreationSettings.h>

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

	static JPH::PhysicsSystem* s_PhysicsSystem = nullptr;
	static JPH::TempAllocatorImpl* s_TempAllocator = nullptr;
	static JPH::JobSystemThreadPool* s_JobSystem = nullptr;
	static BPLayerInterfaceImpl* s_BPLayerInterface = nullptr;
	static ObjectVsBroadPhaseLayerFilterImpl* s_ObjectVsBroadphaseFilter = nullptr;
	static ObjectLayerPairFilterImpl* s_ObjectVsObjectFilter = nullptr;

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

		JPH::BodyInterface& body_interface = s_PhysicsSystem->GetBodyInterface();

		auto& rb_pool = registry.view<me::components::RigidBodyComponent>();
		for (size_t i = 0; i < rb_pool.size(); ++i) {
			auto e = rb_pool.entity_map[i];
			auto& rb = rb_pool.components[i];
			auto* transform = registry.try_get_component<me::components::TransformComponent>(e);

			// Extract collider types
			auto* box_col = registry.try_get_component<me::components::BoxColliderComponent>(e);
			auto* sphere_col = registry.try_get_component<me::components::SphereColliderComponent>(e);

			if (transform && (box_col || sphere_col)) {

				JPH::ShapeRefC shape;

				if (box_col) {
					// --- BOX COLLIDER MATH ---
					float ext_x = std::abs(box_col->half_extents.x * transform->scale.x);
					float ext_y = std::abs(box_col->half_extents.y * transform->scale.y);
					float ext_z = std::abs(box_col->half_extents.z * transform->scale.z);

					if (ext_x <= 0.001f || ext_y <= 0.001f || ext_z <= 0.001f) {
						me::logger::warn("Skipping Box Entity " + std::to_string(e) + " - Physics Colliders cannot have a thickness of zero!");
						continue;
					}

					JPH::BoxShapeSettings shape_settings(JPH::Vec3(ext_x, ext_y, ext_z));
					JPH::ShapeSettings::ShapeResult shape_result = shape_settings.Create();

					if (!shape_result.IsValid()) {
						me::logger::error("Jolt failed to create box shape for Entity " + std::to_string(e));
						continue;
					}
					shape = shape_result.Get();
				} else if (sphere_col) {
					// --- SPHERE COLLIDER MATH ---
					float scaled_radius = std::abs(sphere_col->radius * transform->scale.x);

					if (scaled_radius <= 0.001f) {
						me::logger::warn("Skipping Sphere Entity " + std::to_string(e) + " - Radius cannot be zero!");
						continue;
					}

					JPH::SphereShapeSettings shape_settings(scaled_radius);
					JPH::ShapeSettings::ShapeResult shape_result = shape_settings.Create();

					if (!shape_result.IsValid()) {
						me::logger::error("Jolt failed to create sphere shape for Entity " + std::to_string(e));
						continue;
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
				if (motion_type == JPH::EMotionType::Dynamic) {
					body_settings.mMassPropertiesOverride.mMass = std::max(rb.mass, 0.001f);
					body_settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
				}

				JPH::Body* body = body_interface.CreateBody(body_settings);

				if (body == nullptr) {
					me::logger::error("Jolt returned a null body for Entity " + std::to_string(e));
					continue;
				}

				rb.runtime_body_id = body->GetID().GetIndexAndSequenceNumber();
				body_interface.AddBody(body->GetID(), JPH::EActivation::Activate);
			}
		}

		s_PhysicsSystem->OptimizeBroadPhase();
		me::logger::info("Physics Simulation Started");
	}

	void on_stop() {
		// Clean up the physics simulation entirely
		delete s_PhysicsSystem; s_PhysicsSystem = nullptr;
		delete s_ObjectVsObjectFilter; s_ObjectVsObjectFilter = nullptr;
		delete s_ObjectVsBroadphaseFilter; s_ObjectVsBroadphaseFilter = nullptr;
		delete s_BPLayerInterface; s_BPLayerInterface = nullptr;
		delete s_JobSystem; s_JobSystem = nullptr;
		delete s_TempAllocator; s_TempAllocator = nullptr;

		me::logger::info("Physics Simulation Stopped");
	}

	void update(me::Registry& registry, float dt) {
		if (!s_PhysicsSystem) return;

		// Skip physics step if dt is zero
		if (dt <= 0.0f) return;

		const int collision_steps = 1;
		s_PhysicsSystem->Update(dt, collision_steps, s_TempAllocator, s_JobSystem);

		JPH::BodyInterface& body_interface = s_PhysicsSystem->GetBodyInterface();
		auto& rb_pool = registry.view<me::components::RigidBodyComponent>();

		for (size_t i = 0; i < rb_pool.size(); ++i) {
			auto e = rb_pool.entity_map[i];
			auto& rb = rb_pool.components[i];

			if (rb.type != me::components::RigidBodyType::Static && rb.runtime_body_id != 0xFFFFFFFF) {
				auto* transform = registry.try_get_component<me::components::TransformComponent>(e);
				if (transform) {
					JPH::BodyID id(rb.runtime_body_id);

					if (!body_interface.IsAdded(id)) continue;

					JPH::RVec3 pos = body_interface.GetPosition(id);
					JPH::Quat rot = body_interface.GetRotation(id);

					transform->position = { pos.GetX(), pos.GetY(), pos.GetZ() };

					Quaternion ray_quat = { rot.GetX(), rot.GetY(), rot.GetZ(), rot.GetW() };
					Vector3 euler = QuaternionToEuler(ray_quat);
					transform->rotation = { euler.x * RAD2DEG, euler.y * RAD2DEG, euler.z * RAD2DEG };

					transform->is_dirty = true;
				}
			}
		}
	}

	void draw_debug(me::Registry& registry) {
		auto& rb_pool = registry.view<me::components::RigidBodyComponent>();

		for (size_t i = 0; i < rb_pool.size(); ++i) {
			auto e = rb_pool.entity_map[i];
			auto& rb = rb_pool.components[i];
			auto* transform = registry.try_get_component<me::components::TransformComponent>(e);

			auto* box_col = registry.try_get_component<me::components::BoxColliderComponent>(e);
			auto* sphere_col = registry.try_get_component<me::components::SphereColliderComponent>(e);

			if (transform && (box_col || sphere_col)) {
				::Color wire_color = { 0, 228, 48, 255 }; // Green (Dynamic)

				if (rb.type == me::components::RigidBodyType::Static)
					wire_color = { 230, 41, 55, 255 }; // Red (Static)

				if (rb.type == me::components::RigidBodyType::Kinematic)
					wire_color = { 0, 121, 241, 255 }; // Blue (Kinematic)

				rlPushMatrix();

				// Inject entity's transform matrix directly into the GPU
				Matrix glMatrix = MatrixTranspose(transform->model_matrix);
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

	// ... (Keep your other physics functions) ...

	void set_linear_velocity(me::entity::entity_id e, float x, float y, float z) {
		if (!s_PhysicsSystem) return; // Safety check in case we aren't in Play Mode

		auto& reg = me::get_registry();
		auto* rb = reg.try_get_component<me::components::RigidBodyComponent>(e);

		// Make sure the entity actually has a rigid body that was spawned by Jolt
		if (rb && rb->runtime_body_id != 0xFFFFFFFF) {
			JPH::BodyInterface& body_interface = s_PhysicsSystem->GetBodyInterface();
			JPH::BodyID id(rb->runtime_body_id);

			// Ensure the body still exists in the physics world
			if (body_interface.IsAdded(id)) {
				// 1. Wake the body up (in case it fell asleep sitting on the floor)
				body_interface.ActivateBody(id);

				// 2. Apply the velocity
				body_interface.SetLinearVelocity(id, JPH::Vec3(x, y, z));
			}
		}
	}

} // namespace me::physics
