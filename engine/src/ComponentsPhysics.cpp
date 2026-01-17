#include "Entity.hpp"
#include "Components.hpp"
#include "ComponentsInternal.hpp"
#include "Registry.hpp"

#include "raylib.h"

namespace me {

	// -------- Velocity2D --------
	void AddComponent(EntityId e, const me::components::Velocity2D& c) {
		auto& reg = me::detail::Reg();
		if (!IsAlive(e)) return;
		reg.velocities[e] = c;
	}

	bool GetComponent(EntityId e, me::components::Velocity2D& out) {
		auto& reg = me::detail::Reg();
		auto it = reg.velocities.find(e);
		if (it == reg.velocities.end()) return false;
		out = it->second;
		return true;
	}

	void RemoveComponent(EntityId e, me::components::Velocity2D const&) {
		auto& reg = me::detail::Reg();
		reg.velocities.erase(e);
	}

	bool HasVelocity2D(EntityId e) {
		auto& reg = me::detail::Reg();
		return reg.velocities.find(e) != reg.velocities.end();
	}

	// -------- AabbCollider --------
	void AddComponent(EntityId e, const me::components::AabbCollider& c) {
		auto& reg = me::detail::Reg();
		if (!IsAlive(e)) return;
		reg.colliders[e] = c;
	}

	bool GetComponent(EntityId e, me::components::AabbCollider& out) {
		auto& reg = me::detail::Reg();
		auto it = reg.colliders.find(e);
		if (it == reg.colliders.end()) return false;
		out = it->second;
		return true;
	}

	void RemoveComponent(EntityId e, me::components::AabbCollider const&) {
		auto& reg = me::detail::Reg();
		reg.colliders.erase(e);
	}

	bool HasAabbCollider(EntityId e) {
		auto& reg = me::detail::Reg();
		return reg.colliders.find(e) != reg.colliders.end();
	}

	// -------- CircleCollider --------
	void AddComponent(EntityId e, const me::components::CircleCollider& c) {
		auto& reg = me::detail::Reg();
		if (!IsAlive(e)) return;
		reg.circleColliders[e] = c;
	}

	bool GetComponent(EntityId e, me::components::CircleCollider& out) {
		auto& reg = me::detail::Reg();
		auto it = reg.circleColliders.find(e);
		if (it == reg.circleColliders.end()) return false;
		out = it->second;
		return true;
	}

	void RemoveComponent(EntityId e, me::components::CircleCollider const&) {
		auto& reg = me::detail::Reg();
		reg.circleColliders.erase(e);
	}

	bool HasCircleCollider(EntityId e) {
		auto& reg = me::detail::Reg();
		return reg.circleColliders.find(e) != reg.circleColliders.end();
	}

} // namespace me

namespace me::detail {

	void ForEachVelocity(const std::function<void(me::EntityId, const me::components::Velocity2D&)>& fn) {
		me::detail::Reg().ForEachVelocity(fn);
	}

	void ForEachCollider(const std::function<void(me::EntityId, const me::components::AabbCollider&)>& fn) {
		me::detail::Reg().ForEachCollider(fn);
	}

	void ForEachCircleCollider(const std::function<void(me::EntityId, const me::components::CircleCollider&)>& fn) {
		me::detail::Reg().ForEachCircleCollider(fn);
	}

	void OnEntityDestroyed_Physics(me::EntityId e) {
		auto& r = Reg();
		r.velocities.erase(e);
		r.colliders.erase(e);
		r.circleColliders.erase(e);
	}

} // namespace me::detail
