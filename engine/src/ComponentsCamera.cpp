#include "Entity.hpp"
#include "Components.hpp"
#include "ComponentsInternal.hpp"
#include "Registry.hpp"

#include <raylib.h>

#include <unordered_map>

namespace me {

	void AddComponent(EntityId e, const me::components::Camera2D& c) {
		auto& reg = me::detail::Reg();
		if (!IsAlive(e)) return;
		reg.cameras[e] = c;
	}

	bool GetComponent(EntityId e, me::components::Camera2D& out) {
		auto& reg = me::detail::Reg();
		auto it = reg.cameras.find(e);
		if (it == reg.cameras.end()) return false;
		out = it->second;
		return true;
	}

	void RemoveComponent(EntityId e, me::components::Camera2D const&) {
		auto& reg = me::detail::Reg();
		reg.cameras.erase(e);
	}

	bool HasCamera2D(EntityId e) {
		auto& reg = me::detail::Reg();
		return reg.cameras.find(e) != reg.cameras.end();
	}

} // namespace me

namespace me::detail {

	void ForEachCamera(const std::function<void(me::EntityId, const me::components::Camera2D&)>& fn) {
		me::detail::Reg().ForEachCamera(fn);
	}

	void OnEntityDestroyed_Camera(me::EntityId e) {
		me::detail::Reg().cameras.erase(e);
	}

} // namespace me::detail
