#include "Entity.hpp"
#include "Components.hpp"
#include "ComponentsInternal.hpp"
#include "Registry.hpp"

#include <raylib.h>

namespace me {

	// SpriteSheet
	void AddComponent(EntityId e, const me::components::SpriteSheet& c) {
		auto& reg = me::detail::Reg();
		if (!IsAlive(e)) return;
		reg.animSheets[e] = c;
	}
	bool GetComponent(EntityId e, me::components::SpriteSheet& out) {
		auto& reg = me::detail::Reg();
		auto it = reg.animSheets.find(e);
		if (it == reg.animSheets.end()) return false;
		out = it->second; return true;
	}
	void RemoveComponent(EntityId e, me::components::SpriteSheet const&) {
		me::detail::Reg().animSheets.erase(e);
	}

	bool HasSpriteSheet(EntityId e) {
		auto& reg = me::detail::Reg();
		return reg.animSheets.find(e) != reg.animSheets.end();
	}

	// AnimationPlayer
	void AddComponent(EntityId e, const me::components::AnimationPlayer& c) {
		auto& reg = me::detail::Reg();
		if (!IsAlive(e)) return;
		reg.animPlayers[e] = c;
	}
	bool GetComponent(EntityId e, me::components::AnimationPlayer& out) {
		auto& reg = me::detail::Reg();
		auto it = reg.animPlayers.find(e);
		if (it == reg.animPlayers.end()) return false;
		out = it->second; return true;
	}
	void RemoveComponent(EntityId e, me::components::AnimationPlayer const&) {
		me::detail::Reg().animPlayers.erase(e);
	}

	bool HasAnimationPlayer(EntityId e) {
		auto& reg = me::detail::Reg();
		return reg.animPlayers.find(e) != reg.animPlayers.end();
	}

}

namespace me::detail {
	// Optional iteration helpers if you like symmetry with sprites/cameras:
	void ForEachAnimSheet(const std::function<void(me::EntityId, const me::components::SpriteSheet&)>& fn) {
		for (auto& kv : Reg().animSheets) fn(kv.first, kv.second);
	}

	void ForEachAnimPlayer(const std::function<void(me::EntityId, me::components::AnimationPlayer&)>& fn) {
		for (auto& kv : Reg().animPlayers) fn(kv.first, kv.second);
	}

	void OnEntityDestroyed_Animation(me::EntityId e) {
		Reg().animSheets.erase(e);
		Reg().animPlayers.erase(e);
	}

} // namespace me::detail
