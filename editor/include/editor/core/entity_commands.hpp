#pragma once

// Undoable structural operations: add/remove component, create/delete/duplicate
// entity. All of them drive off the engine component registry (me::ecs), so any
// component the engine can serialize is automatically supported.
//
// Entity ids in mini-ecs are never reused, which is what makes storing ids in
// the history safe: a stale id can only ever refer to a dead entity, so a
// command whose target has vanished degrades to a no-op instead of corrupting
// another entity. The one wrinkle is that an entity restored by Undo gets a
// NEW id; each command updates its own stored id so its Redo stays paired.

#include "editor/core/icommands.hpp"

#include <mini-ecs/registry.hpp>
#include <mini-engine-raylib/ecs/component_registry.hpp>
#include <mini-engine-raylib/ecs/components.hpp>

#include <string>
#include <vector>

namespace editor {

	// =========================================================================
	// ADD COMPONENT (by registry meta name, e.g. "Material")
	// =========================================================================
	class AddComponentCommand : public ICommand {
	public:
		AddComponentCommand(me::Registry& reg, me::entity::entity_id entity, std::string meta_name)
			: m_Reg(&reg), m_Entity(entity), m_Meta(me::ecs::find(meta_name)) {}

		void Execute() override {
			if (!m_Meta || !m_Reg->is_alive(m_Entity)) return;
			// Loading an empty json object constructs the component with defaults.
			if (!m_Meta->has(*m_Reg, m_Entity)) m_Meta->load(*m_Reg, m_Entity, me::ecs::json::object());
		}

		void Undo() override {
			if (!m_Meta || !m_Reg->is_alive(m_Entity)) return;
			if (m_Meta->has(*m_Reg, m_Entity)) m_Meta->remove(*m_Reg, m_Entity);
		}

	private:
		me::Registry* m_Reg;
		me::entity::entity_id m_Entity;
		const me::ecs::ComponentMeta* m_Meta;
	};

	// =========================================================================
	// REMOVE COMPONENT — snapshots the component so Undo can restore it
	// (asset-backed components reload their native handles from the saved paths)
	// =========================================================================
	class RemoveComponentCommand : public ICommand {
	public:
		RemoveComponentCommand(me::Registry& reg, me::entity::entity_id entity, std::string meta_name)
			: m_Reg(&reg), m_Entity(entity), m_Meta(me::ecs::find(meta_name)) {
			if (m_Meta && m_Reg->is_alive(m_Entity) && m_Meta->has(*m_Reg, m_Entity))
				m_Saved = m_Meta->save(*m_Reg, m_Entity);
		}

		void Execute() override {
			if (!m_Meta || !m_Reg->is_alive(m_Entity)) return;
			if (m_Meta->has(*m_Reg, m_Entity)) m_Meta->remove(*m_Reg, m_Entity);
		}

		void Undo() override {
			if (!m_Meta || m_Saved.is_null() || !m_Reg->is_alive(m_Entity)) return;
			if (!m_Meta->has(*m_Reg, m_Entity)) m_Meta->load(*m_Reg, m_Entity, m_Saved);
		}

	private:
		me::Registry* m_Reg;
		me::entity::entity_id m_Entity;
		const me::ecs::ComponentMeta* m_Meta;
		me::ecs::json m_Saved; // null until the ctor captured a live component
	};

	// =========================================================================
	// DELETE ENTITY — snapshots every component + hierarchy links.
	// Undo recreates the entity (new id), reattaches it to its old parent and
	// re-claims the children that were orphaned by the delete.
	// =========================================================================
	class DeleteEntityCommand : public ICommand {
	public:
		DeleteEntityCommand(me::Registry& reg, me::entity::entity_id entity)
			: m_Reg(&reg), m_Entity(entity) {}

		void Execute() override {
			if (!m_Reg->is_alive(m_Entity)) return;

			// Snapshot once, on the first Execute (Redo reuses it).
			if (m_Snapshot.is_null()) {
				m_Snapshot = me::ecs::json::object();
				for (const auto& m : me::ecs::components())
					if (m.has(*m_Reg, m_Entity)) m_Snapshot[m.name] = m.save(*m_Reg, m_Entity);

				if (auto* t = m_Reg->try_get_component<me::components::TransformComponent>(m_Entity)) {
					m_Parent = t->parent;
					m_Children = t->children;
				}
			}

			// Same teardown the editor always did: unlink from the parent and
			// orphan the children so they survive as root entities.
			if (auto* t = m_Reg->try_get_component<me::components::TransformComponent>(m_Entity)) {
				if (t->parent != me::entity::null)
					if (auto* p = m_Reg->try_get_component<me::components::TransformComponent>(t->parent))
						p->remove_child(m_Entity);
				for (auto child : t->children)
					if (auto* ct = m_Reg->try_get_component<me::components::TransformComponent>(child))
						ct->parent = me::entity::null;
			}
			m_Reg->destroy_entity(m_Entity);
		}

		void Undo() override {
			if (m_Snapshot.is_null()) return;

			auto restored = m_Reg->create_entity();
			m_Entity = restored.get_id(); // future Redo deletes the restored entity

			for (const auto& m : me::ecs::components())
				if (m_Snapshot.contains(m.name)) m.load(*m_Reg, m_Entity, m_Snapshot[m.name]);

			// Transform::load only reads the local TRS — re-link the hierarchy here.
			if (auto* t = m_Reg->try_get_component<me::components::TransformComponent>(m_Entity)) {
				t->parent = me::entity::null;
				if (m_Parent != me::entity::null && m_Reg->is_alive(m_Parent)) {
					if (auto* p = m_Reg->try_get_component<me::components::TransformComponent>(m_Parent)) {
						t->parent = m_Parent;
						p->add_child(m_Entity);
					}
				}
				t->children.clear();
				for (auto child : m_Children) {
					if (!m_Reg->is_alive(child)) continue;
					if (auto* ct = m_Reg->try_get_component<me::components::TransformComponent>(child)) {
						ct->parent = m_Entity;
						t->add_child(child);
					}
				}
			}
		}

		me::entity::entity_id entity_id() const { return m_Entity; }

	private:
		me::Registry* m_Reg;
		me::entity::entity_id m_Entity;
		me::ecs::json m_Snapshot; // null until first Execute
		me::entity::entity_id m_Parent = me::entity::null;
		std::vector<me::entity::entity_id> m_Children;
	};

	// =========================================================================
	// DUPLICATE ENTITY — clone every component via the registry. The clone joins
	// the source's parent but never claims the source's children (a raw Transform
	// copy would alias them, leaving two parents fighting over one child).
	// =========================================================================
	class DuplicateEntityCommand : public ICommand {
	public:
		DuplicateEntityCommand(me::Registry& reg, me::entity::entity_id source)
			: m_Reg(&reg), m_Source(source) {}

		void Execute() override {
			if (!m_Reg->is_alive(m_Source)) return;

			auto clone = m_Reg->create_entity();
			m_Clone = clone.get_id();
			me::ecs::clone_entity(*m_Reg, m_Source, m_Clone);

			if (auto* t = m_Reg->try_get_component<me::components::TransformComponent>(m_Clone)) {
				t->children.clear(); // never steal the source's children
				if (t->parent != me::entity::null) {
					auto* p = m_Reg->try_get_component<me::components::TransformComponent>(t->parent);
					if (p) p->add_child(m_Clone);
					else   t->parent = me::entity::null;
				}
			}

			if (auto* tag = m_Reg->try_get_component<me::components::TagComponent>(m_Clone))
				tag->name += " (Clone)";
			else
				m_Reg->add_component<me::components::TagComponent>(m_Clone, { "Entity (Clone)" });
		}

		void Undo() override {
			if (m_Clone == me::entity::null || !m_Reg->is_alive(m_Clone)) return;
			if (auto* t = m_Reg->try_get_component<me::components::TransformComponent>(m_Clone)) {
				if (t->parent != me::entity::null)
					if (auto* p = m_Reg->try_get_component<me::components::TransformComponent>(t->parent))
						p->remove_child(m_Clone);
				for (auto child : t->children)
					if (auto* ct = m_Reg->try_get_component<me::components::TransformComponent>(child))
						ct->parent = me::entity::null;
			}
			m_Reg->destroy_entity(m_Clone);
		}

		me::entity::entity_id clone_id() const { return m_Clone; }

	private:
		me::Registry* m_Reg;
		me::entity::entity_id m_Source;
		me::entity::entity_id m_Clone = me::entity::null;
	};

	// =========================================================================
	// REPARENT — hierarchy drag-drop and "Unparent". Stores both sides of the
	// move (parent + the local position that keeps the world position intact).
	// =========================================================================
	class ReparentCommand : public ICommand {
	public:
		ReparentCommand(me::Registry& reg, me::entity::entity_id entity,
			me::entity::entity_id new_parent, Vector3 new_local_pos)
			: m_Reg(&reg), m_Entity(entity), m_NewParent(new_parent), m_NewPos(new_local_pos) {
			if (auto* t = m_Reg->try_get_component<me::components::TransformComponent>(entity)) {
				m_OldParent = t->parent;
				m_OldPos = t->position;
			}
		}

		void Execute() override { apply(m_NewParent, m_NewPos); }
		void Undo() override { apply(m_OldParent, m_OldPos); }

	private:
		void apply(me::entity::entity_id parent, const Vector3& pos) {
			if (!m_Reg->is_alive(m_Entity)) return;
			auto* t = m_Reg->try_get_component<me::components::TransformComponent>(m_Entity);
			if (!t) return;

			if (t->parent != me::entity::null)
				if (auto* p = m_Reg->try_get_component<me::components::TransformComponent>(t->parent))
					p->remove_child(m_Entity);

			t->parent = me::entity::null;
			if (parent != me::entity::null && m_Reg->is_alive(parent)) {
				if (auto* p = m_Reg->try_get_component<me::components::TransformComponent>(parent)) {
					t->parent = parent;
					p->add_child(m_Entity);
				}
			}
			t->position = pos;
			t->is_dirty = true;
		}

		me::Registry* m_Reg;
		me::entity::entity_id m_Entity;
		me::entity::entity_id m_NewParent;
		Vector3 m_NewPos;
		me::entity::entity_id m_OldParent = me::entity::null;
		Vector3 m_OldPos{ 0.0f, 0.0f, 0.0f };
	};

	// =========================================================================
	// REPLACE COMPONENT STATE — swaps a component's serialized snapshot (used
	// for Script attach/detach, where entries change inside one component).
	// A null snapshot means "component absent".
	// =========================================================================
	class ReplaceComponentCommand : public ICommand {
	public:
		ReplaceComponentCommand(me::Registry& reg, me::entity::entity_id entity,
			std::string meta_name, me::ecs::json after)
			: m_Reg(&reg), m_Entity(entity), m_Meta(me::ecs::find(meta_name)), m_After(std::move(after)) {
			if (m_Meta && m_Reg->is_alive(entity) && m_Meta->has(*m_Reg, entity))
				m_Before = m_Meta->save(*m_Reg, entity);
		}

		void Execute() override { apply(m_After); }
		void Undo() override { apply(m_Before); }

	private:
		void apply(const me::ecs::json& state) {
			if (!m_Meta || !m_Reg->is_alive(m_Entity)) return;
			if (m_Meta->has(*m_Reg, m_Entity)) m_Meta->remove(*m_Reg, m_Entity);
			if (!state.is_null()) m_Meta->load(*m_Reg, m_Entity, state);
		}

		me::Registry* m_Reg;
		me::entity::entity_id m_Entity;
		const me::ecs::ComponentMeta* m_Meta;
		me::ecs::json m_Before; // null = component was absent
		me::ecs::json m_After;  // null = remove the component
	};

	// =========================================================================
	// CREATE ENTITY (empty, with Tag + Transform, optionally parented)
	// =========================================================================
	class CreateEntityCommand : public ICommand {
	public:
		CreateEntityCommand(me::Registry& reg, std::string name,
			me::entity::entity_id parent = me::entity::null)
			: m_Reg(&reg), m_Name(std::move(name)), m_Parent(parent) {}

		void Execute() override {
			auto e = m_Reg->create_entity();
			m_Created = e.get_id();
			e.add_component(me::components::TagComponent{ m_Name });

			me::components::TransformComponent tc{ {0,0,0}, {0,0,0}, {1,1,1} };
			if (m_Parent != me::entity::null && m_Reg->is_alive(m_Parent)) {
				if (auto* p = m_Reg->try_get_component<me::components::TransformComponent>(m_Parent)) {
					tc.parent = m_Parent;
					p->add_child(m_Created);
				}
			}
			e.add_component(tc);
		}

		void Undo() override {
			if (m_Created == me::entity::null || !m_Reg->is_alive(m_Created)) return;
			if (auto* t = m_Reg->try_get_component<me::components::TransformComponent>(m_Created)) {
				if (t->parent != me::entity::null)
					if (auto* p = m_Reg->try_get_component<me::components::TransformComponent>(t->parent))
						p->remove_child(m_Created);
				for (auto child : t->children) // children the user parented under it since
					if (auto* ct = m_Reg->try_get_component<me::components::TransformComponent>(child))
						ct->parent = me::entity::null;
			}
			m_Reg->destroy_entity(m_Created);
		}

		me::entity::entity_id created_id() const { return m_Created; }

	private:
		me::Registry* m_Reg;
		std::string m_Name;
		me::entity::entity_id m_Parent;
		me::entity::entity_id m_Created = me::entity::null;
	};

} // namespace editor
