#include "Entity.hpp"
#include "ComponentsInternal.hpp"
#include "Registry.hpp"

#include <raylib.h>

#include <unordered_map>
#include <utility>

namespace me {

	// -------- lifecycle --------
	EntityId CreateEntity(const char* name) {
		auto& reg = me::detail::Reg();
		EntityId id = reg.nextId++;
		me::detail::Registry::EntityRecord rec{};
		rec.alive = true;
		rec.transform = me::components::Transform2D{};
		if (name && *name) {
			rec.name = name;
			reg.nameIndex[rec.name] = id; // keep name -> id in sync
		}
		reg.entities.emplace(id, std::move(rec));
		return id;
	}

	bool IsAlive(EntityId e) {
		auto& reg = me::detail::Reg();
		auto* rec = reg.Lookup(e);
		return rec && rec->alive;
	}

	void DestroyEntity(EntityId e) {
		auto& reg = me::detail::Reg();

		// remove from name index (if present)
		if (auto* rec = reg.Lookup(e)) {
			if (!rec->name.empty()) {
				auto it = reg.nameIndex.find(rec->name);
				if (it != reg.nameIndex.end() && it->second == e) reg.nameIndex.erase(it);
			}
		}

		// notify components
		me::detail::OnEntityDestroyed(e);
		me::detail::OnEntityDestroyed_Camera(e);
		me::detail::OnEntityDestroyed_Physics(e);
		me::detail::OnEntityDestroyed_Animation(e);

		reg.EraseAllForEntity(e);
		reg.entities.erase(e);
	}

	void DestroyAllEntities() {
		auto& reg = me::detail::Reg();

		for (const auto& kv : reg.entities) {
			me::detail::OnEntityDestroyed(kv.first);
			me::detail::OnEntityDestroyed_Camera(kv.first);
			me::detail::OnEntityDestroyed_Physics(kv.first);
			me::detail::OnEntityDestroyed_Animation(kv.first);
		}

		reg.entities.clear();
		reg.sprites.clear();
		reg.cameras.clear();
		reg.velocities.clear();
		reg.colliders.clear();
		reg.nameIndex.clear();
	}

	// -------- Transform2D component (stored inside entity record) --------
	void AddComponent(EntityId e, const me::components::Transform2D& c) {
		auto& reg = me::detail::Reg();
		auto* rec = reg.Lookup(e);
		if (!rec) return;
		rec->transform = c;
	}

	bool GetComponent(EntityId e, me::components::Transform2D& out) {
		auto& reg = me::detail::Reg();
		auto* rec = reg.Lookup(e);
		if (!rec) return false;
		out = rec->transform;
		return true;
	}

	void RemoveComponent(EntityId e, me::components::Transform2D const&) {
		auto& reg = me::detail::Reg();
		auto* rec = reg.Lookup(e);
		if (!rec) return;
		rec->transform = me::components::Transform2D{};
	}

	bool HasTransform(EntityId e) {
		auto& reg = me::detail::Reg();
		return reg.Lookup(e) != nullptr;
	}

	// -------- Names --------
	const char* GetName(EntityId e) {
		auto& reg = me::detail::Reg();
		auto* rec = reg.Lookup(e);
		if (!rec) return "";
		return rec->name.c_str();
	}

	void SetName(EntityId e, const char* name) {
		auto& reg = me::detail::Reg();
		auto* rec = reg.Lookup(e);
		if (!rec) return;

		// erase old mapping (if any)
		if (!rec->name.empty()) {
			auto it = reg.nameIndex.find(rec->name);
			if (it != reg.nameIndex.end() && it->second == e) reg.nameIndex.erase(it);
		}

		// set new name + mapping
		rec->name = (name && *name) ? name : "";
		if (!rec->name.empty()) reg.nameIndex[rec->name] = e;
	}

	me::Entity FindEntity(const char* name) {
		auto& reg = me::detail::Reg();
		if (!name || !*name) return {};
		auto it = reg.nameIndex.find(name);
		if (it == reg.nameIndex.end()) return {};
		if (!IsAlive(it->second)) return {};
		return me::Entity(it->second);
	}
}
