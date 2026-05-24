#pragma once
#include <mini-ecs/entity.hpp>
#include <mini-ecs/registry.hpp>

namespace editor {

	class SceneHierarchyPanel {
	public:
		SceneHierarchyPanel() = default;

		void set_context(me::Registry* context);
		void on_imgui_render();

		me::entity::entity_id get_selected_entity() const { return m_SelectionContext; }
		void set_selected_entity(me::entity::entity_id entity) { m_SelectionContext = entity; }

	private:
		void draw_entity_node(me::entity::entity_id entity);

	private:
		me::Registry* m_Context = nullptr;
		me::entity::entity_id m_SelectionContext = 0xFFFFFFFF;
	};

}