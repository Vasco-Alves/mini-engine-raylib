#pragma once

#include "Registry.hpp"
#include "Components.hpp" 

namespace me {

	// Lifecycle
	EntityId CreateEntity(const char* name = nullptr);
	bool     IsAlive(EntityId e);
	void     DestroyEntity(EntityId e);
	void     DestroyAllEntities();

	const char* GetName(EntityId e);
	void        SetName(EntityId e, const char* name);

	// High-level wrapper
	class Entity {
	public:
		Entity() = default;
		explicit Entity(EntityId id) : m_id(id) {}

		static Entity Create(const char* name = nullptr) { return Entity(CreateEntity(name)); }
		bool IsValid() const { return m_id != 0 && IsAlive(m_id); }
		EntityId Id() const { return m_id; }

		// Generic Add
		template <typename T>
		Entity& Add(const T& component) {
			me::detail::Reg().AddComponent<T>(m_id, component);
			return *this;
		}

		// Generic Get
		template <typename T>
		bool Get(T& out) const {
			T* ptr = me::detail::Reg().TryGetComponent<T>(m_id);
			if (ptr) { out = *ptr; return true; }
			return false;
		}

		// Pointer access (fast)
		template <typename T>
		T* TryGet() const {
			return me::detail::Reg().TryGetComponent<T>(m_id);
		}

		template <typename T>
		Entity& Remove() {
			me::detail::Reg().RemoveComponent<T>(m_id);
			return *this;
		}

		template <typename T>
		bool Has() const {
			return me::detail::Reg().HasComponent<T>(m_id);
		}

		void        Destroy() { if (m_id) DestroyEntity(m_id); m_id = 0; }
		const char* Name() const { return GetName(m_id); }
		Entity& SetName(const char* n) { ::me::SetName(m_id, n); return *this; }

	private:
		EntityId m_id = 0;
	};

	me::Entity FindEntity(const char* name);
}