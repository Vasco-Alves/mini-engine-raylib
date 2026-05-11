#pragma once

#include <mini-ecs/registry.hpp>
#include <mini-ecs/entity.hpp>

namespace editor {

	class SceneHierarchyPanel {
	public:
		SceneHierarchyPanel() = default;

		// Gives the panel a pointer to the active ECS world
		void set_context(me::Registry* context);

		// Draws both the Hierarchy and the Inspector
		void on_imgui_render();

		me::entity::entity_id get_selected_entity() const { return m_SelectionContext; }
		void set_selected_entity(me::entity::entity_id entity) { m_SelectionContext = entity; }

	private:
		void draw_entity_node(me::entity::entity_id entity);
		void draw_components(me::entity::entity_id entity);

		me::Registry* m_Context = nullptr;
		me::entity::entity_id m_SelectionContext = 0xFFFFFFFF;
	};

} // namespace editor
