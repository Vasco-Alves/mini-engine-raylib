#pragma once

#include "Pool.hpp"

#include <unordered_map>
#include <string>
#include <typeindex>
#include <memory>
#include <functional>

namespace me::detail {

	class Registry {
	public:
		struct EntityRecord {
			bool alive = false;
			std::string name;
		};

		std::unordered_map<me::EntityId, EntityRecord> entities;
		std::unordered_map<std::string, me::EntityId> nameIndex;
		me::EntityId nextId = 1;

	private:
		std::unordered_map<std::type_index, std::unique_ptr<IPool>> m_pools;

	public:
		inline EntityRecord* Lookup(me::EntityId e) {
			if (e == 0) return nullptr;
			auto it = entities.find(e);
			return (it == entities.end()) ? nullptr : &it->second;
		}

		// --- Generic Component API ---
		template <typename T>
		Pool<T>* GetPool() {
			auto typeId = std::type_index(typeid(T));
			if (m_pools.find(typeId) == m_pools.end()) {
				m_pools[typeId] = std::make_unique<Pool<T>>();
			}
			return static_cast<Pool<T>*>(m_pools[typeId].get());
		}

		// Helper: Try to get a pool without creating it (for read-only checks)
		template <typename T>
		Pool<T>* TryGetPool() const {
			auto typeId = std::type_index(typeid(T));
			auto it = m_pools.find(typeId);
			if (it == m_pools.end()) return nullptr;
			return static_cast<Pool<T>*>(it->second.get());
		}

		template <typename T>
		void AddComponent(me::EntityId e, const T& c) {
			GetPool<T>()->Add(e, c);
		}

		template <typename T>
		T& GetComponent(me::EntityId e) {
			return GetPool<T>()->Get(e);
		}

		template <typename T>
		T* TryGetComponent(me::EntityId e) {
			return GetPool<T>()->TryGet(e);
		}

		template <typename T>
		void RemoveComponent(me::EntityId e) {
			GetPool<T>()->Remove(e);
		}

		template <typename T>
		bool HasComponent(me::EntityId e) {
			return GetPool<T>()->Has(e);
		}

		// Wipe entity from ALL pools
		void EraseAllForEntity(me::EntityId e) {
			for (auto& pair : m_pools)
				pair.second->Remove(e);
		}

		inline void ForEachEntity(const std::function<void(me::EntityId)>& fn) const {
			for (const auto& kv : entities)
				fn(kv.first);
		}
	};

	Registry& Reg();
}