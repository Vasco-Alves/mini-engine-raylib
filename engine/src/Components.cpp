#include "Entity.hpp"
#include "Components.hpp"
#include "ComponentsInternal.hpp"
#include "Registry.hpp"

#include <raylib.h>

#include <unordered_map>

namespace me {

	// SpriteRenderer
	void AddComponent(EntityId e, const me::components::SpriteRenderer& c) {
		auto& reg = me::detail::Reg();
		if (!IsAlive(e)) return;
		reg.sprites[e] = c;
	}

	bool GetComponent(EntityId e, me::components::SpriteRenderer& out) {
		auto& reg = me::detail::Reg();
		auto it = reg.sprites.find(e);
		if (it == reg.sprites.end()) return false;
		out = it->second;
		return true;
	}

	void RemoveComponent(EntityId e, const me::components::SpriteRenderer&) {
		//auto& reg = me::detail::Reg();
		//reg.sprites.erase(e);

		auto& reg = me::detail::Reg();
		auto it = reg.sprites.find(e);
		if (it == reg.sprites.end()) return;

		me::assets::Release(it->second.tex);

		reg.sprites.erase(it);
	}


	bool HasSpriteRenderer(EntityId e) {
		auto& reg = me::detail::Reg();
		return reg.sprites.find(e) != reg.sprites.end();
	}

} // namespace me

namespace me::detail {

	void ForEachSprite(const std::function<void(me::EntityId, const me::components::SpriteRenderer&)>& fn) {
		me::detail::Reg().ForEachSprite(fn);
	}

	void OnEntityDestroyed(EntityId e) {
		//me::detail::Reg().sprites.erase(e);
		auto& reg = me::detail::Reg();

		auto it = reg.sprites.find(e);
		if (it != reg.sprites.end()) {
			me::assets::Release(it->second.tex);
			reg.sprites.erase(it);
		}
	}

} // namespace me::detail
