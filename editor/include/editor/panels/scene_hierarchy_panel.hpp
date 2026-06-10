#pragma once
#include <mini-ecs/entity.hpp>
#include <mini-ecs/registry.hpp>

#include "editor/core/icommands.hpp"

namespace editor {

	class SceneHierarchyPanel {
	public:
		SceneHierarchyPanel() = default;

		void set_context(me::Registry* context);
		void on_imgui_render(editor::CommandHistory& command_history);

		me::Entity get_selected_entity() const {
			if (m_SelectionContext == me::entity::null || m_Context == nullptr) {
				return me::Entity(); // Returns an invalid entity
			}
			return me::Entity(m_SelectionContext, m_Context);
		}

		void set_selected_entity(me::entity::entity_id entity) { m_SelectionContext = entity; }
		void set_selected_entity(me::Entity entity) { m_SelectionContext = entity.get_id(); }

		// Starts inline-renaming `entity` (F2 / double-click / context menu).
		void begin_rename(me::entity::entity_id entity);

	private:
		void draw_entity_node(me::entity::entity_id entity, editor::CommandHistory& command_history);
		void draw_rename_field(me::entity::entity_id entity, editor::CommandHistory& command_history);

	private:
		me::Registry* m_Context = nullptr;
		me::entity::entity_id m_SelectionContext = me::entity::null;

		// --- Inline rename state ---
		me::entity::entity_id m_RenamingEntity = me::entity::null;
		char m_RenameBuffer[256] = {};
		bool m_RenameFocusPending = false;
	};

}
