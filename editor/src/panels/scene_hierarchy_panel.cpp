#include "editor/panels/scene_hierarchy_panel.hpp"
#include <mini-engine-raylib/ecs/components.hpp>
#include <imgui.h>
#include <string>

namespace editor {

	void SceneHierarchyPanel::set_context(me::Registry* context) {
		m_Context = context;
		m_SelectionContext = 0xFFFFFFFF; // Reset selection when context changes
	}

	void SceneHierarchyPanel::on_imgui_render() {
		if (!m_Context) return;

		// --- HIERARCHY WINDOW ---
		ImGui::Begin("Scene Hierarchy");

		auto& transform_pool = m_Context->view<me::components::TransformComponent>();

		for (size_t i = 0; i < transform_pool.size(); ++i) {
			me::entity::entity_id entity = transform_pool.entity_map[i];
			draw_entity_node(entity);
		}

		// Deselect if clicking in empty space
		if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered()) {
			m_SelectionContext = 0xFFFFFFFF;
		}

		ImGui::End();

		// --- INSPECTOR WINDOW ---
		ImGui::Begin("Inspector");
		if (m_SelectionContext != 0xFFFFFFFF) {
			draw_components(m_SelectionContext);
		} else {
			ImGui::Text("Select an entity to view its properties.");
		}
		ImGui::End();
	}

	void SceneHierarchyPanel::draw_entity_node(me::entity::entity_id entity) {
		const char* display_name;
		char buffer[64];

		auto* tag = m_Context->try_get_component<me::components::TagComponent>(entity);
		if (tag && !tag->name.empty()) {
			display_name = tag->name.c_str();
		} else {
			snprintf(buffer, sizeof(buffer), "Entity %u", entity);
			display_name = buffer;
		}

		ImGuiTreeNodeFlags flags = ((m_SelectionContext == entity) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
		flags |= ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_Leaf;

		bool opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity, flags, "%s", display_name);

		if (ImGui::IsItemClicked()) {
			m_SelectionContext = entity;
		}

		if (opened) {
			ImGui::TreePop();
		}
	}

	void SceneHierarchyPanel::draw_components(me::entity::entity_id entity) {
		// 1. TAG COMPONENT
		auto* tag = m_Context->try_get_component<me::components::TagComponent>(entity);
		if (tag) {
			char buffer[256];
			strncpy(buffer, tag->name.c_str(), sizeof(buffer));
			if (ImGui::InputText("Name", buffer, sizeof(buffer))) {
				tag->name = std::string(buffer);
			}
			ImGui::Separator();
		}

		// 2. TRANSFORM COMPONENT
		auto* transform = m_Context->try_get_component<me::components::TransformComponent>(entity);
		if (transform) {
			ImGui::Text("Transform");
			ImGui::DragFloat3("Position", &transform->position.x, 0.1f);
			ImGui::DragFloat3("Rotation", &transform->rotation.x, 1.0f);
			ImGui::DragFloat3("Scale", &transform->scale.x, 0.1f);
			ImGui::Separator();
		}

		// 3. SHAPE 3D COMPONENT
		auto* shape = m_Context->try_get_component<me::components::Shape3DComponent>(entity);
		if (shape) {
			ImGui::Text("Shape 3D");
			const char* types[] = { "Cube", "Sphere", "Plane" };
			int current_type = static_cast<int>(shape->type);
			if (ImGui::Combo("Primitive", &current_type, types, 3)) {
				shape->type = static_cast<me::components::Shape3DComponent::Type>(current_type);
			}

			float color[4] = { shape->color.r / 255.0f, shape->color.g / 255.0f, shape->color.b / 255.0f, shape->color.a / 255.0f };
			if (ImGui::ColorEdit4("Color", color)) {
				shape->color.r = static_cast<uint8_t>(color[0] * 255.0f);
				shape->color.g = static_cast<uint8_t>(color[1] * 255.0f);
				shape->color.b = static_cast<uint8_t>(color[2] * 255.0f);
				shape->color.a = static_cast<uint8_t>(color[3] * 255.0f);
			}
			ImGui::Checkbox("Wireframe", &shape->wireframe);
			ImGui::Separator();
		}

		// 4. MODEL 3D COMPONENT
		auto* mod = m_Context->try_get_component<me::components::Model3DComponent>(entity);
		if (mod) {
			ImGui::Text("3D Model");
			float color[4] = { mod->tint.r / 255.0f, mod->tint.g / 255.0f, mod->tint.b / 255.0f, mod->tint.a / 255.0f };
			if (ImGui::ColorEdit4("Tint", color)) {
				mod->tint.r = static_cast<uint8_t>(color[0] * 255.0f);
				mod->tint.g = static_cast<uint8_t>(color[1] * 255.0f);
				mod->tint.b = static_cast<uint8_t>(color[2] * 255.0f);
				mod->tint.a = static_cast<uint8_t>(color[3] * 255.0f);
			}
			ImGui::Separator();
		}
	}
}
