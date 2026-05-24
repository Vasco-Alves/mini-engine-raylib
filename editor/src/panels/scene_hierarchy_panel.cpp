#include "editor/panels/scene_hierarchy_panel.hpp"
#include <imgui.h>
#include <string>
#include <raylib.h>
#include <raymath.h>
#include <mini-engine-raylib/ecs/components.hpp>
#include <mini-engine-raylib/ecs/audio_components.hpp>
#include <mini-engine-raylib/assets/assets.hpp>
#include <mini-engine-raylib/audio/audio.hpp>

namespace editor {

	void SceneHierarchyPanel::set_context(me::Registry* context) {
		m_Context = context;
		m_SelectionContext = 0xFFFFFFFF;
	}

	void SceneHierarchyPanel::on_imgui_render() {
		if (!m_Context) return;

		ImGui::Begin("Scene Hierarchy");

		auto& transform_pool = m_Context->view<me::components::TransformComponent>();

		// 1. Draw only Root Entities (entities without a parent)
		for (size_t i = 0; i < transform_pool.size(); ++i) {
			me::entity::entity_id entity = transform_pool.entity_map[i];
			auto& t = transform_pool.components[i];

			// Only kick off the drawing chain if it's a top-level object
			if (t.parent == me::entity::null) {
				draw_entity_node(entity);
			}
		}

		// Deselect if clicking in empty space
		if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered()) {
			m_SelectionContext = 0xFFFFFFFF;
		}

		// Right-Click Empty Space -> Create Entity
		if (ImGui::BeginPopupContextWindow("HierarchyContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
			if (ImGui::MenuItem("Create Empty Entity")) {
				auto e = m_Context->create_entity();
				e.add_component(me::components::TagComponent{ "New Entity" });
				e.add_component(me::components::TransformComponent{ {0,0,0}, {0,0,0}, {1,1,1} });
				m_SelectionContext = e.get_id(); // Auto-select new entity
			}
			ImGui::EndPopup();
		}

		// 2. Drag & Drop into empty space to "Unparent" (Move to Root)
		ImGui::Dummy(ImGui::GetContentRegionAvail()); // Invisible drop zone taking up remaining window space
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY")) {
				me::entity::entity_id dropped_entity = *(const me::entity::entity_id*)payload->Data;
				auto* dropped_transform = m_Context->try_get_component<me::components::TransformComponent>(dropped_entity);

				// Detach from current parent
				if (dropped_transform && dropped_transform->parent != me::entity::null) {
					auto* old_parent = m_Context->try_get_component<me::components::TransformComponent>(dropped_transform->parent);
					if (old_parent) {
						old_parent->remove_child(dropped_entity);
						// MATH FIX: Convert Local Space back to World Space so it doesn't teleport!
						dropped_transform->position = Vector3Transform(dropped_transform->position, old_parent->model_matrix);
					}

					dropped_transform->parent = me::entity::null;
				}
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::End();
	}

	void SceneHierarchyPanel::draw_entity_node(me::entity::entity_id entity) {
		if (!m_Context->is_alive(entity)) return;

		auto* transform = m_Context->try_get_component<me::components::TransformComponent>(entity);
		if (!transform) return;

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
		flags |= ImGuiTreeNodeFlags_SpanAvailWidth;

		// If this entity has no children, render it as a flat leaf (no dropdown arrow)
		if (transform->children.empty()) {
			flags |= ImGuiTreeNodeFlags_Leaf;
		}

		bool opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity, flags, "%s", display_name);

		if (ImGui::IsItemClicked()) {
			m_SelectionContext = entity;
		}

		// ==========================================
		// DRAG & DROP SOURCE (Pick up the entity)
		// ==========================================
		if (ImGui::BeginDragDropSource()) {
			ImGui::SetDragDropPayload("HIERARCHY_ENTITY", &entity, sizeof(me::entity::entity_id));
			ImGui::Text("Move %s", display_name);
			ImGui::EndDragDropSource();
		}

		// ==========================================
		// DRAG & DROP TARGET (Drop onto another entity to parent it)
		// ==========================================
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY")) {
				me::entity::entity_id dropped_entity = *(const me::entity::entity_id*)payload->Data;

				// Make sure we aren't dropping it onto itself
				if (dropped_entity != entity) {
					auto* dropped_transform = m_Context->try_get_component<me::components::TransformComponent>(dropped_entity);

					// SAFETY CHECK: Prevent Cyclic Dependencies!
					bool is_cyclic = false;
					me::entity::entity_id curr = entity;
					while (curr != me::entity::null) {
						if (curr == dropped_entity) {
							is_cyclic = true;
							break;
						}
						auto* curr_t = m_Context->try_get_component<me::components::TransformComponent>(curr);
						curr = curr_t ? curr_t->parent : me::entity::null;
					}

					if (!is_cyclic && dropped_transform) {
						Vector3 trueWorldPos = {
							dropped_transform->model_matrix.m12,
							dropped_transform->model_matrix.m13,
							dropped_transform->model_matrix.m14
						};

						// Convert World -> Local relative to the NEW parent
						Matrix invParent = MatrixInvert(transform->model_matrix);
						Vector3 localPos = Vector3Transform(trueWorldPos, invParent);

						dropped_transform->position = localPos;

						// Attach
						if (dropped_transform->parent != me::entity::null) {
							auto* old_parent = m_Context->try_get_component<me::components::TransformComponent>(dropped_transform->parent);
							if (old_parent) old_parent->remove_child(dropped_entity);
						}
						dropped_transform->parent = entity;
						transform->add_child(dropped_entity);
					}
				}
			}
			ImGui::EndDragDropTarget();
		}

		// --- Right-Click Specific Entity ---
		if (ImGui::BeginPopupContextItem()) {

			if (ImGui::MenuItem("Create Empty Child")) {
				auto e = m_Context->create_entity();
				e.add_component(me::components::TagComponent{ "New Child Entity" });

				me::components::TransformComponent tc;
				tc.parent = entity;
				tc.position = { 0,0,0 };
				e.add_component(tc);

				transform->add_child(e.get_id());
				m_SelectionContext = e.get_id();
			}

			// Unparent Option
			if (transform->parent != me::entity::null) {
				if (ImGui::MenuItem("Unparent (Move to Root)")) {
					auto* old_parent = m_Context->try_get_component<me::components::TransformComponent>(transform->parent);
					if (old_parent) {
						old_parent->remove_child(entity);
						//transform->position = Vector3Transform(transform->position, old_parent->model_matrix);
						transform->position = { transform->model_matrix.m12, transform->model_matrix.m13, transform->model_matrix.m14 };
					}
					transform->parent = me::entity::null;
				}
			}

			ImGui::Separator();

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
			if (ImGui::MenuItem("Delete Entity")) {

				// Unlink from parent before dying
				if (transform->parent != me::entity::null) {
					auto* old_parent = m_Context->try_get_component<me::components::TransformComponent>(transform->parent);
					if (old_parent) old_parent->remove_child(entity);
				}

				m_Context->destroy_entity(entity);

				if (m_SelectionContext == entity) {
					m_SelectionContext = 0xFFFFFFFF;
				}
			}
			ImGui::PopStyleColor();
			ImGui::EndPopup();
		}

		// ==========================================
		// RECURSIVE DRAWING
		// ==========================================
		if (opened) {
			// Copy the vector so iterator doesn't crash if an item is deleted while looping
			auto children_copy = transform->children;
			for (auto child_id : children_copy) {
				draw_entity_node(child_id);
			}
			ImGui::TreePop();
		}
	}
}
