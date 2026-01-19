#pragma once

#include <cstdint>
#include <unordered_map>
#include <iostream>

namespace me {
	using EntityId = std::uint32_t;
}

namespace me::detail {

	// Interface for type erasure (lets us store Pool<T>* as IPool*)
	struct IPool {
		virtual ~IPool() = default;
		virtual void Remove(me::EntityId e) = 0;
		virtual bool Has(me::EntityId e) const = 0;
	};

	template <typename T>
	struct Pool : public IPool {
		std::unordered_map<me::EntityId, T> data;

		void Add(me::EntityId e, const T& component) {
			data[e] = component;
		}

		T& Get(me::EntityId e) {
			return data.at(e);
		}

		T* TryGet(me::EntityId e) {
			auto it = data.find(e);
			return (it != data.end()) ? &it->second : nullptr;
		}

		void Remove(me::EntityId e) override {
			data.erase(e);
		}

		bool Has(me::EntityId e) const override {
			return data.find(e) != data.end();
		}
	};

}