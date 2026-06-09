#pragma once

#include <nlohmann/json.hpp>
#include <mini-ecs/entity.hpp>

#include <functional>
#include <string>
#include <vector>

namespace me { class Registry; }

namespace me::ecs {

	using json = nlohmann::ordered_json;

	// Metadata that lets generic engine code (scene serialization, entity
	// duplication, native-handle cleanup) operate on a component type without
	// hard-coding the list of components in each place.
	struct ComponentMeta {
		std::string name; // key used in scene JSON, e.g. "MeshRenderer"

		std::function<bool(Registry&, entity::entity_id)> has;
		std::function<json(Registry&, entity::entity_id)> save;                       // component -> json object
		std::function<void(Registry&, entity::entity_id, const json&)> load;          // json -> add component
		std::function<void(Registry&, entity::entity_id, entity::entity_id)> clone;   // (reg, src, dst)
		std::function<void(Registry&, entity::entity_id)> on_destroy;                 // empty if no native handle
	};

	// The single source of truth: every component the engine can persist, in a
	// stable order. To support a new component, add one entry in component_registry.cpp
	// and serialization, duplication and handle cleanup all pick it up.
	const std::vector<ComponentMeta>& components();

	// Copy every registered component present on `src` onto `dst`.
	void clone_entity(Registry& reg, entity::entity_id src, entity::entity_id dst);

	// Release native (GPU/audio) handles owned by `e`, just before it is destroyed.
	void release_native_handles(Registry& reg, entity::entity_id e);

} // namespace me::ecs
