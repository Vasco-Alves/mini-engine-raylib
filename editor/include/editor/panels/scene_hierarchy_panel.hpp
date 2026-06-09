#pragma once
#include <mini-ecs/entity.hpp>
#include <mini-ecs/registry.hpp>

namespace editor {

	class SceneHierarchyPanel {
	public:
		SceneHierarchyPanel() = default;

		void set_context(me::Registry* context);
		void on_imgui_render();

		me::Entity get_selected_entity() const {
			if (m_SelectionContext == me::entity::null || m_Context == nullptr) {
				return me::Entity(); // Returns an invalid entity
			}
			return me::Entity(m_SelectionContext, m_Context);
		}

		void set_selected_entity(me::entity::entity_id entity) { m_SelectionContext = entity; }
		void set_selected_entity(me::Entity entity) { m_SelectionContext = entity.get_id(); }

	private:
		void draw_entity_node(me::entity::entity_id entity);

	private:
		me::Registry* m_Context = nullptr;
		me::entity::entity_id m_SelectionContext = me::entity::null;
	};

}