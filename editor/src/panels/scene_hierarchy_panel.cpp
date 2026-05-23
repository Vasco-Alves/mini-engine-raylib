#include "editor/panels/scene_hierarchy_panel.hpp"

#include <mini-engine-raylib/ecs/components.hpp>
#include <mini-engine-raylib/ecs/physics_components.hpp>
#include <mini-engine-raylib/ecs/script_component.hpp>
#include <mini-engine-raylib/assets/assets.hpp>
#include <mini-engine-raylib/core/logger.hpp>
#include <imgui.h>
#include <string>
#include <filesystem>

namespace editor {

	// Safe deferred deletion tracker to prevent ECS iteration crashes
	static me::entity::entity_id s_EntityToDelete = 0xFFFFFFFF;

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

		ImGui::End();

		// --- SAFE DEFERRED DELETION ---
		if (s_EntityToDelete != 0xFFFFFFFF) {
			if (m_SelectionContext == s_EntityToDelete) {
				m_SelectionContext = 0xFFFFFFFF;
			}
			m_Context->destroy_entity(s_EntityToDelete);
			s_EntityToDelete = 0xFFFFFFFF;
		}

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

		// --- Right-Click Specific Entity -> Delete ---
		if (ImGui::BeginPopupContextItem()) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.2f, 0.2f, 1.0f)); // Red text
			if (ImGui::MenuItem("Delete Entity")) {
				s_EntityToDelete = entity; // Mark for deletion outside the loop
			}
			ImGui::PopStyleColor();
			ImGui::EndPopup();
		}

		if (opened) {
			ImGui::TreePop();
		}
	}

	void SceneHierarchyPanel::draw_components(me::entity::entity_id entity) {
		const ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_SpanAvailWidth;

		// TAG COMPONENT
		if (auto* tag = m_Context->try_get_component<me::components::TagComponent>(entity)) {
			char buffer[256];
			memset(buffer, 0, sizeof(buffer));
			strncpy(buffer, tag->name.c_str(), sizeof(buffer) - 1);

			ImGui::Text("Name");
			ImGui::SameLine();
			ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);

			if (ImGui::InputText("##Tag", buffer, sizeof(buffer))) {
				tag->name = std::string(buffer);
			}

			ImGui::PopItemWidth();
			ImGui::Dummy(ImVec2(0, 10));
		}

		// TRANSFORM COMPONENT
		if (auto* transform = m_Context->try_get_component<me::components::TransformComponent>(entity)) {
			if (ImGui::TreeNodeEx("Transform", treeNodeFlags)) {
				ImGui::DragFloat3("Position", &transform->position.x, 0.1f);
				ImGui::DragFloat3("Rotation", &transform->rotation.x, 1.0f);
				ImGui::DragFloat3("Scale", &transform->scale.x, 0.1f);
				ImGui::TreePop();
			}
		}

		// SHAPE 3D COMPONENT
		if (auto* shape = m_Context->try_get_component<me::components::Shape3DComponent>(entity)) {
			ImGui::PushID("Shape3D");

			float button_size = ImGui::GetFrameHeight();
			bool opened = ImGui::TreeNodeEx("Shape 3D", treeNodeFlags);

			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - button_size);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
			bool remove_component = ImGui::Button("X", ImVec2(button_size, button_size));
			ImGui::PopStyleColor();

			if (remove_component) m_Context->remove_component<me::components::Shape3DComponent>(entity);

			if (opened) {
				if (!remove_component) {
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
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}

		// MODEL 3D COMPONENT
		if (auto* modelComp = m_Context->try_get_component<me::components::Model3DComponent>(entity)) {
			ImGui::PushID("Model3D");

			float button_size = ImGui::GetFrameHeight();
			bool opened = ImGui::TreeNodeEx("Model 3D", treeNodeFlags);

			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - button_size);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
			bool remove_component = ImGui::Button("X", ImVec2(button_size, button_size));
			ImGui::PopStyleColor();

			if (remove_component) {
				if (modelComp->model.handle != 0) me::assets::release(modelComp->model);
				m_Context->remove_component<me::components::Model3DComponent>(entity);
			}

			if (opened) {
				if (!remove_component) {
					ImGui::Text("Mesh");
					ImGui::SameLine();
					ImGui::Button("Drag .obj / .glb Here", ImVec2(ImGui::GetContentRegionAvail().x, 0));

					if (ImGui::BeginDragDropTarget()) {
						if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
							const char* dropped_path = (const char*)payload->Data;
							std::filesystem::path fp = dropped_path;
							if (fp.extension() == ".glb" || fp.extension() == ".obj") {
								if (modelComp->model.handle != 0) me::assets::release(modelComp->model);
								modelComp->model = me::assets::load_model(dropped_path);
								me::logger::info(std::string("Successfully swapped model to: ") + dropped_path);
							} else {
								me::logger::warn("You can only drop .obj or .glb files onto a Model3DComponent.");
							}
						}
						ImGui::EndDragDropTarget();
					}
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}

		// LIGHT COMPONENT
		if (auto* light = m_Context->try_get_component<me::components::LightComponent>(entity)) {
			ImGui::PushID("Light");

			float button_size = ImGui::GetFrameHeight();
			bool opened = ImGui::TreeNodeEx("Light", treeNodeFlags);

			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - button_size);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
			bool remove_component = ImGui::Button("X", ImVec2(button_size, button_size));
			ImGui::PopStyleColor();

			if (remove_component) m_Context->remove_component<me::components::LightComponent>(entity);

			if (opened) {
				if (!remove_component) {
					float color[3] = { light->color.r / 255.0f, light->color.g / 255.0f, light->color.b / 255.0f };
					if (ImGui::ColorEdit3("Color", color)) {
						light->color.r = static_cast<uint8_t>(color[0] * 255.0f);
						light->color.g = static_cast<uint8_t>(color[1] * 255.0f);
						light->color.b = static_cast<uint8_t>(color[2] * 255.0f);
					}
					ImGui::DragFloat("Intensity", &light->intensity, 0.1f, 0.0f, 100.0f);
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}

		// DIRECTIONAL LIGHT COMPONENT
		if (auto* light = m_Context->try_get_component<me::components::DirectionalLightComponent>(entity)) {
			ImGui::PushID("DirLight");

			float button_size = ImGui::GetFrameHeight();
			bool opened = ImGui::TreeNodeEx("Directional Light", treeNodeFlags);

			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - button_size);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
			bool remove_component = ImGui::Button("X", ImVec2(button_size, button_size));
			ImGui::PopStyleColor();

			if (remove_component) m_Context->remove_component<me::components::DirectionalLightComponent>(entity);

			if (opened) {
				if (!remove_component) {
					float color[3] = { light->color.r / 255.0f, light->color.g / 255.0f, light->color.b / 255.0f };
					if (ImGui::ColorEdit3("Color", color)) {
						light->color.r = static_cast<uint8_t>(color[0] * 255.0f);
						light->color.g = static_cast<uint8_t>(color[1] * 255.0f);
						light->color.b = static_cast<uint8_t>(color[2] * 255.0f);
					}
					ImGui::DragFloat("Intensity", &light->intensity, 0.1f, 0.0f, 100.0f);
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}

		// RIGID BODY COMPONENT
		if (auto* rb = m_Context->try_get_component<me::components::RigidBodyComponent>(entity)) {
			ImGui::PushID("RigidBody");

			float button_size = ImGui::GetFrameHeight();
			bool opened = ImGui::TreeNodeEx("Rigid Body", treeNodeFlags);

			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - button_size);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
			bool remove_component = ImGui::Button("X", ImVec2(button_size, button_size));
			ImGui::PopStyleColor();

			if (remove_component) m_Context->remove_component<me::components::RigidBodyComponent>(entity);

			if (opened) {
				if (!remove_component) {
					const char* body_types[] = { "Static", "Dynamic", "Kinematic" };
					int current_type = static_cast<int>(rb->type);
					if (ImGui::Combo("Body Type", &current_type, body_types, 3)) {
						rb->type = static_cast<me::components::RigidBodyType>(current_type);
					}

					if (rb->type == me::components::RigidBodyType::Dynamic) {
						ImGui::DragFloat("Mass", &rb->mass, 0.1f, 0.001f, 1000.0f);
					}
					ImGui::DragFloat("Bounciness", &rb->bounciness, 0.05f, 0.0f, 1.0f);
					ImGui::DragFloat("Friction", &rb->friction, 0.05f, 0.0f, 10.0f);
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}

		// BOX COLLIDER COMPONENT
		if (auto* col = m_Context->try_get_component<me::components::BoxColliderComponent>(entity)) {
			ImGui::PushID("BoxCollider");

			float button_size = ImGui::GetFrameHeight();
			bool opened = ImGui::TreeNodeEx("Box Collider", treeNodeFlags);

			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - button_size);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
			bool remove_component = ImGui::Button("X", ImVec2(button_size, button_size));
			ImGui::PopStyleColor();

			if (remove_component) m_Context->remove_component<me::components::BoxColliderComponent>(entity);

			if (opened) {
				if (!remove_component) {
					ImGui::DragFloat3("Half Extents", &col->half_extents.x, 0.1f, 0.01f, 100.0f);
					ImGui::Checkbox("Show Debug Wireframe", &col->show_debug);
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}

		// SPHERE COLLIDER COMPONENT
		if (auto* col = m_Context->try_get_component<me::components::SphereColliderComponent>(entity)) {
			ImGui::PushID("SphereCollider");

			float button_size = ImGui::GetFrameHeight();
			bool opened = ImGui::TreeNodeEx("Sphere Collider", treeNodeFlags);

			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - button_size);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
			bool remove_component = ImGui::Button("X", ImVec2(button_size, button_size));
			ImGui::PopStyleColor();

			if (remove_component) m_Context->remove_component<me::components::SphereColliderComponent>(entity);

			if (opened) {
				if (!remove_component) {
					ImGui::DragFloat("Radius", &col->radius, 0.1f, 0.01f, 100.0f);
					ImGui::Checkbox("Show Debug Wireframe", &col->show_debug);
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}

		// SCRIPT COMPONENTS
		auto* script_comp = m_Context->try_get_component<me::components::ScriptComponent>(entity);
		if (script_comp) {
			ImGui::PushID("Scripts");
			bool opened = ImGui::TreeNodeEx("Lua Scripts", treeNodeFlags);

			if (opened) {
				size_t script_to_delete = (size_t)-1;
				float button_size = ImGui::GetFrameHeight();

				for (size_t i = 0; i < script_comp->scripts.size(); ++i) {
					ImGui::PushID((int)i);
					ImGui::TextColored(ImVec4(0.2f, 0.6f, 0.9f, 1.0f), "%s", script_comp->scripts[i].path.c_str());
					ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - button_size);

					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
					if (ImGui::Button("X", ImVec2(button_size, button_size))) script_to_delete = i;
					ImGui::PopStyleColor();
					ImGui::PopID();
				}

				if (script_to_delete != (size_t)-1) {
					script_comp->scripts.erase(script_comp->scripts.begin() + script_to_delete);
					if (script_comp->scripts.empty()) {
						m_Context->remove_component<me::components::ScriptComponent>(entity);
						script_comp = nullptr;
					}
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}

		// SCRIPT DRAG & DROP ZONE
		ImGui::Separator();
		ImGui::Dummy(ImVec2(0, 10));

		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.14f, 0.18f, 1.0f));
		ImGui::BeginChild("DropZone", ImVec2(0, 60), true, ImGuiWindowFlags_NoScrollbar);

		auto windowWidth = ImGui::GetWindowSize().x;
		auto textWidth = ImGui::CalcTextSize("Drop .lua Script Here").x;
		ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
		ImGui::SetCursorPosY((60 - ImGui::GetTextLineHeight()) * 0.5f);
		ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Drop .lua Script Here");

		ImGui::EndChild();
		ImGui::PopStyleColor();

		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
				const char* dropped_path = (const char*)payload->Data;
				std::string path_str(dropped_path);

				if (path_str.find(".lua") != std::string::npos) {
					if (!script_comp) {
						me::components::ScriptComponent sc;
						sc.scripts.push_back({ path_str });
						m_Context->add_component<me::components::ScriptComponent>(entity, sc);
					} else {
						bool already_attached = false;
						for (const auto& existing : script_comp->scripts) {
							if (existing.path == path_str) {
								already_attached = true;
								break;
							}
						}
						if (!already_attached) script_comp->scripts.push_back({ path_str });
					}
				}
			}
			ImGui::EndDragDropTarget();
		}

		// ADD COMPONENT BUTTON
		ImGui::Dummy(ImVec2(0, 10));
		ImGui::Separator();
		ImGui::Dummy(ImVec2(0, 10));

		if (ImGui::Button("Add Component", ImVec2(ImGui::GetContentRegionAvail().x, 30))) {
			ImGui::OpenPopup("AddComponentPopup");
		}

		if (ImGui::BeginPopup("AddComponentPopup")) {

			// Shape 3D Component
			if (!m_Context->try_get_component<me::components::Shape3DComponent>(entity)) {
				if (ImGui::MenuItem("3D Primitive Shape")) {
					m_Context->add_component<me::components::Shape3DComponent>(entity,
						me::components::Shape3DComponent{ me::components::Shape3DComponent::Cube, me::Color::white }
					);
					ImGui::CloseCurrentPopup();
				}
			}

			// Model 3D Component
			if (!m_Context->try_get_component<me::components::Model3DComponent>(entity)) {
				if (ImGui::MenuItem("3D Model")) {
					m_Context->add_component<me::components::Model3DComponent>(entity,
						me::components::Model3DComponent{ 0, me::Color::white }
					);
					ImGui::CloseCurrentPopup();
				}
			}

			// Camera Component
			if (!m_Context->try_get_component<me::components::CameraComponent>(entity)) {
				if (ImGui::MenuItem("Camera")) {
					m_Context->add_component<me::components::CameraComponent>(entity, me::components::CameraComponent{});
					ImGui::CloseCurrentPopup();
				}
			}

			// Light Component
			if (!m_Context->try_get_component<me::components::LightComponent>(entity)) {
				if (ImGui::MenuItem("Light")) {
					m_Context->add_component<me::components::LightComponent>(entity, me::components::LightComponent{});
					ImGui::CloseCurrentPopup();
				}
			}

			// Directional Light Component
			if (!m_Context->try_get_component<me::components::DirectionalLightComponent>(entity)) {
				if (ImGui::MenuItem("Directional Light")) {
					m_Context->add_component<me::components::DirectionalLightComponent>(entity, me::components::DirectionalLightComponent{});
					ImGui::CloseCurrentPopup();
				}
			}

			ImGui::Separator(); // Visual break for physics

			// Rigid Body Component
			if (!m_Context->try_get_component<me::components::RigidBodyComponent>(entity)) {
				if (ImGui::MenuItem("Rigid Body")) {
					m_Context->add_component<me::components::RigidBodyComponent>(entity, me::components::RigidBodyComponent{});
					ImGui::CloseCurrentPopup();
				}
			}

			// Box Collider Component
			if (!m_Context->try_get_component<me::components::BoxColliderComponent>(entity)) {
				if (ImGui::MenuItem("Box Collider")) {
					m_Context->add_component<me::components::BoxColliderComponent>(entity, me::components::BoxColliderComponent{});
					ImGui::CloseCurrentPopup();
				}
			}

			// Sphere Collider Component
			if (!m_Context->try_get_component<me::components::SphereColliderComponent>(entity)) {
				if (ImGui::MenuItem("Sphere Collider")) {
					m_Context->add_component<me::components::SphereColliderComponent>(entity, me::components::SphereColliderComponent{});
					ImGui::CloseCurrentPopup();
				}
			}

			ImGui::EndPopup();
		}
	}

}
