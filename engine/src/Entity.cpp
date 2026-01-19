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

		// Transform2D gets added by default
		reg.AddComponent<me::components::Transform2D>(id, {});

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

		// We assume systems clean up their own external resources (like Physics bodies)
		// via the OnUpdate loops, but for Texture ref-counting, we might need a hook.
		// For now, we rely on Assets::ReleaseUnused() being called periodically, 
		// or we can explicitly check SpriteRenderer here.

		if (auto* sr = reg.TryGetComponent<me::components::SpriteRenderer>(e)) {
			me::assets::Release(sr->tex);
		}
		if (auto* ss = reg.TryGetComponent<me::components::SpriteSheet>(e)) {
			me::assets::Release(ss->tex);
		}

		reg.EraseAllForEntity(e);
		reg.entities.erase(e);
	}

	void DestroyAllEntities() {
		auto& reg = me::detail::Reg();

		// Release assets for all sprites
		auto* spritePool = reg.TryGetPool<me::components::SpriteRenderer>();
		if (spritePool) {
			for (auto& kv : spritePool->data) {
				me::assets::Release(kv.second.tex);
			}
		}
		auto* sheetPool = reg.TryGetPool<me::components::SpriteSheet>();
		if (sheetPool) {
			for (auto& kv : sheetPool->data) {
				me::assets::Release(kv.second.tex);
			}
		}

		reg.entities.clear();
		reg.nameIndex.clear();

		// Clearing entities map is enough, but to be safe we clear pools too
		// We can't iterate m_pools easily from here without friendship or public access,
		// but since Registry is destroyed/cleared on Shutdown, this is mostly for scene transitions.
		// A helper in Registry would be better, but this works for now:
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