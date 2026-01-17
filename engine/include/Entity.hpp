#pragma once

#include <cstdint>

#include "Components.hpp"

namespace me {

	using EntityId = std::uint32_t;

	// -------- Low-level lifecycle --------
	EntityId CreateEntity(const char* name = nullptr);
	bool     IsAlive(EntityId e);
	void     DestroyEntity(EntityId e);
	void     DestroyAllEntities();

	// -------- Name support --------
	const char* GetName(EntityId e);
	void        SetName(EntityId e, const char* name);

	// -------- Component access (overloads per known type) --------
	// Transform2D
	void     AddComponent(EntityId e, const me::components::Transform2D& c);
	bool     GetComponent(EntityId e, me::components::Transform2D& out);
	void     RemoveComponent(EntityId e, me::components::Transform2D const&);
	bool     HasTransform(EntityId e);

	// SpriteRenderer
	void     AddComponent(EntityId e, const me::components::SpriteRenderer& c);
	bool     GetComponent(EntityId e, me::components::SpriteRenderer& out);
	void     RemoveComponent(EntityId e, me::components::SpriteRenderer const&);
	bool     HasSpriteRenderer(EntityId e);

	// Camera2D
	void     AddComponent(EntityId e, const me::components::Camera2D& c);
	bool     GetComponent(EntityId e, me::components::Camera2D& out);
	void     RemoveComponent(EntityId e, me::components::Camera2D const&);
	bool     HasCamera2D(EntityId e);

	// Velocity2D
	void     AddComponent(EntityId e, const me::components::Velocity2D& c);
	bool     GetComponent(EntityId e, me::components::Velocity2D& out);
	void     RemoveComponent(EntityId e, me::components::Velocity2D const&);
	bool     HasVelocity2D(EntityId e);

	// AabbCollider
	void     AddComponent(EntityId e, const me::components::AabbCollider& c);
	bool     GetComponent(EntityId e, me::components::AabbCollider& out);
	void     RemoveComponent(EntityId e, me::components::AabbCollider const&);
	bool     HasAabbCollider(EntityId e);

	// CircleCollider
	void     AddComponent(EntityId e, const me::components::CircleCollider& c);
	bool     GetComponent(EntityId e, me::components::CircleCollider& out);
	void     RemoveComponent(EntityId e, me::components::CircleCollider const&);
	bool     HasCircleCollider(EntityId e);

	// SpriteSheet
	void     AddComponent(EntityId e, const me::components::SpriteSheet& c);
	bool     GetComponent(EntityId e, me::components::SpriteSheet& out);
	void     RemoveComponent(EntityId e, me::components::SpriteSheet const&);
	bool     HasSpriteSheet(EntityId e);

	// AnimationPlayer
	void     AddComponent(EntityId e, const me::components::AnimationPlayer& c);
	bool     GetComponent(EntityId e, me::components::AnimationPlayer& out);
	void     RemoveComponent(EntityId e, me::components::AnimationPlayer const&);
	bool     HasAnimationPlayer(EntityId e);

	// -------- Ergonomic wrapper for game code --------
	class Entity {
	public:
		Entity() = default;
		explicit Entity(EntityId id) : m_id(id) {}

		static Entity Create(const char* name = nullptr) { return Entity(CreateEntity(name)); }

		bool     IsValid() const { return m_id != 0 && IsAlive(m_id); }
		EntityId Id()      const { return m_id; }

		// Add
		Entity& Add(const me::components::Transform2D& c) { AddComponent(m_id, c); return *this; }
		Entity& Add(const me::components::SpriteRenderer& c) { AddComponent(m_id, c); return *this; }
		Entity& Add(const me::components::Camera2D& c) { AddComponent(m_id, c); return *this; }
		Entity& Add(const me::components::Velocity2D& c) { AddComponent(m_id, c); return *this; }
		Entity& Add(const me::components::AabbCollider& c) { AddComponent(m_id, c); return *this; }
		Entity& Add(const me::components::CircleCollider& c) { AddComponent(m_id, c); return *this; }
		Entity& Add(const me::components::SpriteSheet& c) { AddComponent(m_id, c); return *this; }
		Entity& Add(const me::components::AnimationPlayer& c) { AddComponent(m_id, c); return *this; }

		// Get
		bool Get(me::components::Transform2D& out)       const { return GetComponent(m_id, out); }
		bool Get(me::components::SpriteRenderer& out)    const { return GetComponent(m_id, out); }
		bool Get(me::components::Camera2D& out)          const { return GetComponent(m_id, out); }
		bool Get(me::components::Velocity2D& out)        const { return GetComponent(m_id, out); }
		bool Get(me::components::AabbCollider& out)      const { return GetComponent(m_id, out); }
		bool Get(me::components::CircleCollider& out)    const { return GetComponent(m_id, out); }
		bool Get(me::components::SpriteSheet& out)       const { return GetComponent(m_id, out); }
		bool Get(me::components::AnimationPlayer& out)   const { return GetComponent(m_id, out); }

		// Remove
		Entity& Remove(const me::components::Transform2D& c) { RemoveComponent(m_id, c); return *this; }
		Entity& Remove(const me::components::SpriteRenderer& c) { RemoveComponent(m_id, c); return *this; }
		Entity& Remove(const me::components::Camera2D& c) { RemoveComponent(m_id, c); return *this; }
		Entity& Remove(const me::components::Velocity2D& c) { RemoveComponent(m_id, c); return *this; }
		Entity& Remove(const me::components::AabbCollider& c) { RemoveComponent(m_id, c); return *this; }
		Entity& Remove(const me::components::CircleCollider& c) { RemoveComponent(m_id, c); return *this; }
		Entity& Remove(const me::components::SpriteSheet& c) { RemoveComponent(m_id, c); return *this; }
		Entity& Remove(const me::components::AnimationPlayer& c) { RemoveComponent(m_id, c); return *this; }

		void        Destroy() { if (m_id) DestroyEntity(m_id); m_id = 0; }
		const char* Name() const { return ::me::GetName(m_id); }
		Entity& SetName(const char* n) { ::me::SetName(m_id, n); return *this; }

		// Has (convenience)
		/*
		bool HasTransform()     const { return ::me::HasTransform(m_id); }
		bool HasSpriteRenderer() const { return ::me::HasSpriteRenderer(m_id); }
		bool HasCamera2D()      const { return ::me::HasCamera2D(m_id); }
		bool HasVelocity2D()    const { return ::me::HasVelocity2D(m_id); }
		bool HasAabbCollider()  const { return ::me::HasAabbCollider(m_id); }
		*/

	private:
		EntityId m_id = 0;
	};

	// Fast O(1) lookup if you added the name index in the registry
	me::Entity FindEntity(const char* name);
}
