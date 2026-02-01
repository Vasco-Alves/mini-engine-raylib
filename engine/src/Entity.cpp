#include "Entity.hpp"
#include "Registry.hpp"
#include "Components.hpp"

namespace me {

	EntityId CreateEntity(const char* name) {
		auto& reg = me::detail::Reg();
		EntityId id = reg.nextId++;

		me::detail::Registry::EntityRecord rec{};
		rec.alive = true;
		if (name && *name) {
			rec.name = name;
			reg.nameIndex[rec.name] = id;
		}

		reg.entities.emplace(id, std::move(rec));
		reg.AddComponent<me::components::Transform>(id, {}); // Transform gets added by default

		return id;
	}

	bool IsAlive(EntityId e) {
		auto& reg = me::detail::Reg();
		auto* rec = reg.Lookup(e);
		return rec && rec->alive;
	}

	void DestroyEntity(EntityId e) {
		auto& reg = me::detail::Reg();

		if (auto* rec = reg.Lookup(e)) {
			if (!rec->name.empty()) {
				auto it = reg.nameIndex.find(rec->name);
				if (it != reg.nameIndex.end() && it->second == e) reg.nameIndex.erase(it);
			}
		}

		// [FIX] Removed SpriteRenderer/SpriteSheet cleanup because those types don't exist anymore.
		// If MeshRenderer eventually uses Assets, we will add cleanup here later.

		reg.EraseAllForEntity(e);
		reg.entities.erase(e);
	}

	void DestroyAllEntities() {
		auto& reg = me::detail::Reg();

		// [FIX] Removed sprite asset cleanup loop.

		reg.entities.clear();
		reg.nameIndex.clear();

		std::vector<EntityId> all;
		for (auto& kv : reg.entities) all.push_back(kv.first);
		for (auto e : all) reg.EraseAllForEntity(e);
	}

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

		if (!rec->name.empty()) {
			auto it = reg.nameIndex.find(rec->name);
			if (it != reg.nameIndex.end() && it->second == e) reg.nameIndex.erase(it);
		}

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