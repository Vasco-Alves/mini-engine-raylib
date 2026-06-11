#include "editor/panels/scene_hierarchy_panel.hpp"
#include "editor/core/entity_commands.hpp"

#include <imgui.h>
#include <string>
#include <cstdio>
#include <memory>
#include <raylib.h>
#include <raymath.h>
#include <mini-engine-raylib/ecs/components.hpp>
#include <mini-engine-raylib/ecs/audio_components.hpp>
#include <mini-engine-raylib/assets/assets.hpp>
#include <mini-engine-raylib/audio/audio.hpp>

namespace editor {

	void SceneHierarchyPanel::set_context(me::Registry* context) {
		m_Context = context;
		m_SelectionContext = me::entity::null;
		m_RenamingEntity = me::entity::null;
	}

	void SceneHierarchyPanel::begin_rename(me::entity::entity_id entity) {
		if (!m_Context || entity == me::entity::null || !m_Context->is_alive(entity)) return;

		m_RenamingEntity = entity;
		m_RenameFocusPending = true;

		auto* tag = m_Context->try_get_component<me::components::TagComponent>(entity);
		std::string current = (tag && !tag->name.empty()) ? tag->name : ("Entity " + std::to_string(entity));
		snprintf(m_RenameBuffer, sizeof(m_RenameBuffer), "%s", current.c_str());
	}

	void SceneHierarchyPanel::on_imgui_render(editor::CommandHistory& command_history) {
		if (!m_Context) return;

		ImGui::Begin("Scene Hierarchy");

		auto& transform_pool = m_Context->view<me::components::TransformComponent>();

		// 1. Draw only Root Entities (entities without a parent)
		for (size_t i = 0; i < transform_pool.size(); ++i) {
			me::entity::entity_id entity = transform_pool.entity_map[i];
			auto& t = transform_pool.components[i];

			// Only kick off the drawing chain if it's a top-level object
			if (t.parent == me::entity::null) {
				draw_entity_node(entity, command_history);
			}
		}

		// Deselect if clicking in empty space
		if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered()) {
			m_SelectionContext = me::entity::null;
		}

		// Right-Click Empty Space -> Create Entity
		if (ImGui::BeginPopupContextWindow("HierarchyContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
			if (ImGui::MenuItem("Create Empty Entity")) {
				auto cmd = std::make_unique<editor::CreateEntityCommand>(*m_Context, "New Entity");
				auto* raw = cmd.get();
				command_history.AddCommand(std::move(cmd));
				m_SelectionContext = raw->created_id(); // Auto-select new entity
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
						dropped_transform->position = {
							dropped_transform->model_matrix.m12,
							dropped_transform->model_matrix.m13,
							dropped_transform->model_matrix.m14
						};
					}

					dropped_transform->parent = me::entity::null;
				}
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::End();
	}

	// Inline rename: replaces the entity's tree-node label with an InputText.
	// Enter / clicking away commits (undoable); Escape cancels.
	void SceneHierarchyPanel::draw_rename_field(me::entity::entity_id entity, editor::CommandHistory& command_history) {
		ImGui::PushID((int)entity);
		ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);

		if (m_RenameFocusPending) {
			ImGui::SetKeyboardFocusHere();
			m_RenameFocusPending = false;
		}

		bool entered = ImGui::InputText("##rename", m_RenameBuffer, sizeof(m_RenameBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
		bool done = entered || ImGui::IsItemDeactivated();

		if (done) {
			bool cancelled = ImGui::IsKeyPressed(ImGuiKey_Escape);
			if (!cancelled && m_RenameBuffer[0] != '\0') {
				auto* tag = m_Context->try_get_component<me::components::TagComponent>(entity);
				me::components::TagComponent before = tag ? *tag : me::components::TagComponent{ "" };
				me::components::TagComponent after{ std::string(m_RenameBuffer) };

				if (!tag) m_Context->add_component<me::components::TagComponent>(entity, before);
				if (after.name != before.name) {
					command_history.AddCommand(std::make_unique<editor::ModifyComponentCommand<me::components::TagComponent>>(
						me::Entity(entity, m_Context), before, after));
				}
			}
			m_RenamingEntity = me::entity::null;
		}

		ImGui::PopItemWidth();
		ImGui::PopID();
	}

	void SceneHierarchyPanel::draw_entity_node(me::entity::entity_id entity, editor::CommandHistory& command_history) {
		if (!m_Context->is_alive(entity)) return;

		auto* transform = m_Context->try_get_component<me::components::TransformComponent>(entity);
		if (!transform) return;

		// While renaming, the node row becomes a text field (children reappear after).
		if (m_RenamingEntity == entity) {
			draw_rename_field(entity, command_history);
			return;
		}

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

		// Double-click the name -> rename in place
		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
			begin_rename(entity);
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

						// Convert World -> Local relative to the new parent
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
				auto cmd = std::make_unique<editor::CreateEntityCommand>(*m_Context, "New Child Entity", entity);
				auto* raw = cmd.get();
				command_history.AddCommand(std::move(cmd));
				m_SelectionContext = raw->created_id();
			}

			if (ImGui::MenuItem("Rename", "F2")) {
				begin_rename(entity);
			}

			if (ImGui::MenuItem("Duplicate", "Ctrl+D")) {
				auto cmd = std::make_unique<editor::DuplicateEntityCommand>(*m_Context, entity);
				auto* raw = cmd.get();
				command_history.AddCommand(std::move(cmd));
				m_SelectionContext = raw->clone_id();
			}

			// Unparent Option
			if (transform->parent != me::entity::null) {
				if (ImGui::MenuItem("Unparent (Move to Root)")) {
					auto* old_parent = m_Context->try_get_component<me::components::TransformComponent>(transform->parent);
					if (old_parent) {
						old_parent->remove_child(entity);
						transform->position = {
							transform->model_matrix.m12,
							transform->model_matrix.m13,
							transform->model_matrix.m14
						};
					}
					transform->parent = me::entity::null;
				}
			}

			ImGui::Separator();

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
			if (ImGui::MenuItem("Delete Entity", "Del")) {
				// Undoable: snapshots the whole entity (components + hierarchy).
				// Children are orphaned to root, exactly like the Delete key.
				command_history.AddCommand(std::make_unique<editor::DeleteEntityCommand>(*m_Context, entity));

				if (m_SelectionContext == entity) {
					m_SelectionContext = me::entity::null;
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
				draw_entity_node(child_id, command_history);
			}
			ImGui::TreePop();
		}
	}
}
