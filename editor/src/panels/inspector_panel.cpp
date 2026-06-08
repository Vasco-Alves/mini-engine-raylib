#include "editor/panels/inspector_panel.hpp"

#include <mini-engine-raylib/ecs/components.hpp>
#include <mini-engine-raylib/ecs/physics_components.hpp>
#include <mini-engine-raylib/ecs/audio_components.hpp>
#include <mini-engine-raylib/ecs/script_component.hpp>
#include <mini-engine-raylib/assets/assets.hpp>
#include <mini-engine-raylib/audio/audio.hpp>
#include <mini-engine-raylib/core/logger.hpp>
#include <imgui.h>
#include <string>
#include <filesystem>

namespace editor {

	static const ImGuiTreeNodeFlags s_TreeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_SpanAvailWidth;

	void InspectorPanel::on_imgui_render(me::Entity selected_entity, editor::CommandHistory& command_history) {
		ImGui::Begin("Inspector");

		if (selected_entity.is_valid()) {
			draw_tag(selected_entity, command_history);
			draw_transform(selected_entity, command_history);
			draw_shape3d(selected_entity, command_history);
			draw_model3d(selected_entity, command_history);
			draw_material(selected_entity, command_history);
			draw_light(selected_entity, command_history);
			draw_directional_light(selected_entity, command_history);
			draw_rigidbody(selected_entity, command_history);
			draw_box_collider(selected_entity, command_history);
			draw_sphere_collider(selected_entity, command_history);
			draw_audio_source(selected_entity, command_history);
			draw_audio_listener(selected_entity, command_history);
			draw_background_music(selected_entity, command_history);
			draw_script(selected_entity);

			draw_add_component_menu(selected_entity);
		} else {
			ImGui::Text("Select an entity to view its properties.");
		}

		ImGui::End();
	}

	void InspectorPanel::draw_tag(me::Entity entity, editor::CommandHistory& command_history) {
		if (auto* tag = entity.try_get_component<me::components::TagComponent>()) {
			char buffer[256];
			memset(buffer, 0, sizeof(buffer));
			strncpy(buffer, tag->name.c_str(), sizeof(buffer) - 1);

			ImGui::Text("Name");
			ImGui::SameLine();
			ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);

			static me::components::TagComponent start_state;
			bool finished_editing = false;

			if (ImGui::InputText("##Tag", buffer, sizeof(buffer))) {
				tag->name = std::string(buffer);
			}

			if (ImGui::IsItemActivated()) start_state = *tag;
			if (ImGui::IsItemDeactivatedAfterEdit()) finished_editing = true;

			if (finished_editing) {
				auto cmd = std::make_unique<editor::ModifyComponentCommand<me::components::TagComponent>>(
					entity, start_state, *tag
				);
				command_history.AddCommand(std::move(cmd));
			}

			ImGui::PopItemWidth();
			ImGui::Dummy(ImVec2(0, 10));
		}
	}

	void InspectorPanel::draw_transform(me::Entity entity, editor::CommandHistory& command_history) {
		if (auto* transform = entity.try_get_component<me::components::TransformComponent>()) {
			if (ImGui::TreeNodeEx("Transform", s_TreeNodeFlags)) {
				static me::components::TransformComponent start_state;
				bool finished_editing = false;

				// --- Position ---
				ImGui::DragFloat3("Position", &transform->position.x, 0.1f);
				if (ImGui::IsItemActivated()) start_state = *transform;
				if (ImGui::IsItemDeactivatedAfterEdit()) finished_editing = true;

				// --- Rotation ---
				ImGui::DragFloat3("Rotation", &transform->rotation.x, 1.0f);
				if (ImGui::IsItemActivated()) start_state = *transform;
				if (ImGui::IsItemDeactivatedAfterEdit()) finished_editing = true;

				// --- Scale ---
				ImGui::DragFloat3("Scale", &transform->scale.x, 0.1f);
				if (ImGui::IsItemActivated()) start_state = *transform;
				if (ImGui::IsItemDeactivatedAfterEdit()) finished_editing = true;

				if (finished_editing) {
					auto cmd = std::make_unique<editor::ModifyComponentCommand<me::components::TransformComponent>>(
						entity, start_state, *transform
					);
					command_history.AddCommand(std::move(cmd));
				}

				ImGui::TreePop();
			}
		}
	}

	void InspectorPanel::draw_shape3d(me::Entity entity, editor::CommandHistory& command_history) {
		if (auto* shape = entity.try_get_component<me::components::Shape3DComponent>()) {
			ImGui::PushID("Shape3D");

			float button_size = ImGui::GetFrameHeight();
			bool opened = ImGui::TreeNodeEx("Shape 3D", s_TreeNodeFlags);

			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - button_size);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
			bool remove_component = ImGui::Button("X", ImVec2(button_size, button_size));
			ImGui::PopStyleColor();

			if (remove_component) entity.remove_component<me::components::Shape3DComponent>();

			if (opened && !remove_component) {
				static me::components::Shape3DComponent start_state;
				bool finished_editing = false;

				const char* types[] = { "Cube", "Sphere", "Plane" };
				int current_type = static_cast<int>(shape->type);
				if (ImGui::Combo("Primitive", &current_type, types, 3)) {
					shape->type = static_cast<me::components::Shape3DComponent::Type>(current_type);
				}
				if (ImGui::IsItemActivated()) start_state = *shape;
				if (ImGui::IsItemDeactivatedAfterEdit()) finished_editing = true;

				float color[4] = { shape->color.r / 255.0f, shape->color.g / 255.0f, shape->color.b / 255.0f, shape->color.a / 255.0f };
				if (ImGui::ColorEdit4("Color", color)) {
					shape->color.r = static_cast<uint8_t>(color[0] * 255.0f);
					shape->color.g = static_cast<uint8_t>(color[1] * 255.0f);
					shape->color.b = static_cast<uint8_t>(color[2] * 255.0f);
					shape->color.a = static_cast<uint8_t>(color[3] * 255.0f);
				}
				if (ImGui::IsItemActivated()) start_state = *shape;
				if (ImGui::IsItemDeactivatedAfterEdit()) finished_editing = true;

				ImGui::Checkbox("Wireframe", &shape->wireframe);
				if (ImGui::IsItemActivated()) start_state = *shape;
				if (ImGui::IsItemDeactivatedAfterEdit()) finished_editing = true;

				if (finished_editing) {
					auto cmd = std::make_unique<editor::ModifyComponentCommand<me::components::Shape3DComponent>>(
						entity, start_state, *shape
					);
					command_history.AddCommand(std::move(cmd));
				}

				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	}

	// ==========================================
	// NEW MATERIAL PANEL IMPLEMENTATION
	// ==========================================
	void InspectorPanel::draw_material(me::Entity entity, editor::CommandHistory& command_history) {
		if (auto* mat = entity.try_get_component<me::components::MaterialComponent>()) {
			ImGui::PushID("Material");

			float button_size = ImGui::GetFrameHeight();
			bool opened = ImGui::TreeNodeEx("Material", s_TreeNodeFlags);

			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - button_size);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
			bool remove_component = ImGui::Button("X", ImVec2(button_size, button_size));
			ImGui::PopStyleColor();

			if (remove_component) entity.remove_component<me::components::MaterialComponent>();

			if (opened && !remove_component) {
				static me::components::MaterialComponent start_state;
				bool finished_editing = false;

				// Albedo Color
				float color[4] = { mat->albedo.r / 255.0f, mat->albedo.g / 255.0f, mat->albedo.b / 255.0f, mat->albedo.a / 255.0f };
				if (ImGui::ColorEdit4("Albedo", color)) {
					mat->albedo.r = static_cast<uint8_t>(color[0] * 255.0f);
					mat->albedo.g = static_cast<uint8_t>(color[1] * 255.0f);
					mat->albedo.b = static_cast<uint8_t>(color[2] * 255.0f);
					mat->albedo.a = static_cast<uint8_t>(color[3] * 255.0f);
				}
				if (ImGui::IsItemActivated()) start_state = *mat;
				if (ImGui::IsItemDeactivatedAfterEdit()) finished_editing = true;

				// Roughness
				ImGui::SliderFloat("Roughness", &mat->roughness, 0.0f, 1.0f);
				if (ImGui::IsItemActivated()) start_state = *mat;
				if (ImGui::IsItemDeactivatedAfterEdit()) finished_editing = true;

				// Metallic
				ImGui::SliderFloat("Metallic", &mat->metallic, 0.0f, 1.0f);
				if (ImGui::IsItemActivated()) start_state = *mat;
				if (ImGui::IsItemDeactivatedAfterEdit()) finished_editing = true;

				// Emission
				ImGui::DragFloat("Emission Power", &mat->emission_power, 0.1f, 0.0f, 100.0f);
				if (ImGui::IsItemActivated()) start_state = *mat;
				if (ImGui::IsItemDeactivatedAfterEdit()) finished_editing = true;

				// Transmission (Slider 0 to 1)
				ImGui::SliderFloat("Transmission (Glass)", &mat->transmission, 0.0f, 1.0f);
				if (ImGui::IsItemActivated()) start_state = *mat;
				if (ImGui::IsItemDeactivatedAfterEdit()) finished_editing = true;

				// Index of Refraction (Drag 1.0 to 3.0)
				ImGui::DragFloat("IOR", &mat->ior, 0.01f, 1.0f, 3.0f, "%.2f");
				if (ImGui::IsItemActivated()) start_state = *mat;
				if (ImGui::IsItemDeactivatedAfterEdit()) finished_editing = true;

				if (finished_editing) {
					auto cmd = std::make_unique<editor::ModifyComponentCommand<me::components::MaterialComponent>>(
						entity, start_state, *mat
					);
					command_history.AddCommand(std::move(cmd));
				}

				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	}

	void InspectorPanel::draw_model3d(me::Entity entity, editor::CommandHistory& command_history) {
		if (auto* modelComp = entity.try_get_component<me::components::Model3DComponent>()) {
			ImGui::PushID("Model3D");

			float button_size = ImGui::GetFrameHeight();
			bool opened = ImGui::TreeNodeEx("Model 3D", s_TreeNodeFlags);

			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - button_size);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
			bool remove_component = ImGui::Button("X", ImVec2(button_size, button_size));
			ImGui::PopStyleColor();

			if (remove_component) {
				if (modelComp->model.handle != 0) me::assets::release(modelComp->model);
				entity.remove_component<me::components::Model3DComponent>();
			}

			if (opened && !remove_component) {
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
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	}

	void InspectorPanel::draw_light(me::Entity entity, editor::CommandHistory& command_history) {
		if (auto* light = entity.try_get_component<me::components::LightComponent>()) {
			ImGui::PushID("Light");

			float button_size = ImGui::GetFrameHeight();
			bool opened = ImGui::TreeNodeEx("Light", s_TreeNodeFlags);

			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - button_size);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
			bool remove_component = ImGui::Button("X", ImVec2(button_size, button_size));
			ImGui::PopStyleColor();

			if (remove_component) entity.remove_component<me::components::LightComponent>();

			if (opened && !remove_component) {
				static me::components::LightComponent start_state;
				bool finished_editing = false;

				float color[3] = { light->color.r / 255.0f, light->color.g / 255.0f, light->color.b / 255.0f };
				if (ImGui::ColorEdit3("Color", color)) {
					light->color.r = static_cast<uint8_t>(color[0] * 255.0f);
					light->color.g = static_cast<uint8_t>(color[1] * 255.0f);
					light->color.b = static_cast<uint8_t>(color[2] * 255.0f);
				}
				if (ImGui::IsItemActivated()) start_state = *light;
				if (ImGui::IsItemDeactivatedAfterEdit()) finished_editing = true;

				ImGui::DragFloat("Intensity", &light->intensity, 0.1f, 0.0f, 100.0f);
				if (ImGui::IsItemActivated()) start_state = *light;
				if (ImGui::IsItemDeactivatedAfterEdit()) finished_editing = true;

				if (finished_editing) {
					auto cmd = std::make_unique<editor::ModifyComponentCommand<me::components::LightComponent>>(
						entity, start_state, *light
					);
					command_history.AddCommand(std::move(cmd));
				}

				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	}

	void InspectorPanel::draw_directional_light(me::Entity entity, editor::CommandHistory& command_history) {
		if (auto* light = entity.try_get_component<me::components::DirectionalLightComponent>()) {
			ImGui::PushID("DirLight");

			float button_size = ImGui::GetFrameHeight();
			bool opened = ImGui::TreeNodeEx("Directional Light", s_TreeNodeFlags);

			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - button_size);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
			bool remove_component = ImGui::Button("X", ImVec2(button_size, button_size));
			ImGui::PopStyleColor();

			if (remove_component) entity.remove_component<me::components::DirectionalLightComponent>();

			if (opened && !remove_component) {
				static me::components::DirectionalLightComponent start_state;
				bool finished_editing = false;

				float color[3] = { light->color.r / 255.0f, light->color.g / 255.0f, light->color.b / 255.0f };
				if (ImGui::ColorEdit3("Color", color)) {
					light->color.r = static_cast<uint8_t>(color[0] * 255.0f);
					light->color.g = static_cast<uint8_t>(color[1] * 255.0f);
					light->color.b = static_cast<uint8_t>(color[2] * 255.0f);
				}
				if (ImGui::IsItemActivated()) start_state = *light;
				if (ImGui::IsItemDeactivatedAfterEdit()) finished_editing = true;

				ImGui::DragFloat("Intensity", &light->intensity, 0.1f, 0.0f, 100.0f);
				if (ImGui::IsItemActivated()) start_state = *light;
				if (ImGui::IsItemDeactivatedAfterEdit()) finished_editing = true;

				if (finished_editing) {
					auto cmd = std::make_unique<editor::ModifyComponentCommand<me::components::DirectionalLightComponent>>(
						entity, start_state, *light
					);
					command_history.AddCommand(std::move(cmd));
				}

				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	}

	void InspectorPanel::draw_rigidbody(me::Entity entity, editor::CommandHistory& command_history) {
		if (auto* rb = entity.try_get_component<me::components::RigidBodyComponent>()) {
			ImGui::PushID("RigidBody");

			float button_size = ImGui::GetFrameHeight();
			bool opened = ImGui::TreeNodeEx("Rigid Body", s_TreeNodeFlags);

			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - button_size);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
			bool remove_component = ImGui::Button("X", ImVec2(button_size, button_size));
			ImGui::PopStyleColor();

			if (remove_component) entity.remove_component<me::components::RigidBodyComponent>();

			if (opened && !remove_component) {
				static me::components::RigidBodyComponent start_state;
				bool finished_editing = false;

				const char* body_types[] = { "Static", "Dynamic", "Kinematic" };
				int current_type = static_cast<int>(rb->type);
				if (ImGui::Combo("Body Type", &current_type, body_types, 3)) {
					rb->type = static_cast<me::components::RigidBodyType>(current_type);
				}
				if (ImGui::IsItemActivated()) start_state = *rb;
				if (ImGui::IsItemDeactivatedAfterEdit()) finished_editing = true;

				if (rb->type == me::components::RigidBodyType::Dynamic) {
					ImGui::DragFloat("Mass", &rb->mass, 0.1f, 0.001f, 1000.0f);
					if (ImGui::IsItemActivated()) start_state = *rb;
					if (ImGui::IsItemDeactivatedAfterEdit()) finished_editing = true;
				}

				ImGui::DragFloat("Bounciness", &rb->bounciness, 0.05f, 0.0f, 1.0f);
				if (ImGui::IsItemActivated()) start_state = *rb;
				if (ImGui::IsItemDeactivatedAfterEdit()) finished_editing = true;

				ImGui::DragFloat("Friction", &rb->friction, 0.05f, 0.0f, 10.0f);
				if (ImGui::IsItemActivated()) start_state = *rb;
				if (ImGui::IsItemDeactivatedAfterEdit()) finished_editing = true;

				if (finished_editing) {
					auto cmd = std::make_unique<editor::ModifyComponentCommand<me::components::RigidBodyComponent>>(
						entity, start_state, *rb
					);
					command_history.AddCommand(std::move(cmd));
				}

				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	}

	void InspectorPanel::draw_box_collider(me::Entity entity, editor::CommandHistory& command_history) {
		if (auto* col = entity.try_get_component<me::components::BoxColliderComponent>()) {
			ImGui::PushID("BoxCollider");

			float button_size = ImGui::GetFrameHeight();
			bool opened = ImGui::TreeNodeEx("Box Collider", s_TreeNodeFlags);

			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - button_size);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
			bool remove_component = ImGui::Button("X", ImVec2(button_size, button_size));
			ImGui::PopStyleColor();

			if (remove_component) entity.remove_component<me::components::BoxColliderComponent>();

			if (opened && !remove_component) {
				static me::components::BoxColliderComponent start_state;
				bool finished_editing = false;

				ImGui::DragFloat3("Half Extents", &col->half_extents.x, 0.1f, 0.01f, 100.0f);
				if (ImGui::IsItemActivated()) start_state = *col;
				if (ImGui::IsItemDeactivatedAfterEdit()) finished_editing = true;

				ImGui::Checkbox("Show Debug Wireframe", &col->show_debug);
				if (ImGui::IsItemActivated()) start_state = *col;
				if (ImGui::IsItemDeactivatedAfterEdit()) finished_editing = true;

				if (finished_editing) {
					auto cmd = std::make_unique<editor::ModifyComponentCommand<me::components::BoxColliderComponent>>(
						entity, start_state, *col
					);
					command_history.AddCommand(std::move(cmd));
				}

				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	}

	void InspectorPanel::draw_sphere_collider(me::Entity entity, editor::CommandHistory& command_history) {
		if (auto* col = entity.try_get_component<me::components::SphereColliderComponent>()) {
			ImGui::PushID("SphereCollider");

			float button_size = ImGui::GetFrameHeight();
			bool opened = ImGui::TreeNodeEx("Sphere Collider", s_TreeNodeFlags);

			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - button_size);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
			bool remove_component = ImGui::Button("X", ImVec2(button_size, button_size));
			ImGui::PopStyleColor();

			if (remove_component) entity.remove_component<me::components::SphereColliderComponent>();

			if (opened && !remove_component) {
				static me::components::SphereColliderComponent start_state;
				bool finished_editing = false;

				ImGui::DragFloat("Radius", &col->radius, 0.1f, 0.01f, 100.0f);
				if (ImGui::IsItemActivated()) start_state = *col;
				if (ImGui::IsItemDeactivatedAfterEdit()) finished_editing = true;

				ImGui::Checkbox("Show Debug Wireframe", &col->show_debug);
				if (ImGui::IsItemActivated()) start_state = *col;
				if (ImGui::IsItemDeactivatedAfterEdit()) finished_editing = true;

				if (finished_editing) {
					auto cmd = std::make_unique<editor::ModifyComponentCommand<me::components::SphereColliderComponent>>(
						entity, start_state, *col
					);
					command_history.AddCommand(std::move(cmd));
				}

				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	}

	void InspectorPanel::draw_audio_source(me::Entity entity, editor::CommandHistory& command_history) {
		if (auto* audio = entity.try_get_component<me::components::AudioSourceComponent>()) {
			ImGui::PushID("AudioSource");

			float button_size = ImGui::GetFrameHeight();
			bool opened = ImGui::TreeNodeEx("Audio Source", s_TreeNodeFlags);

			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - button_size);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
			bool remove_component = ImGui::Button("X", ImVec2(button_size, button_size));
			ImGui::PopStyleColor();

			if (remove_component) {
				if (audio->clip.handle != 0) me::audio::release(audio->clip);
				entity.remove_component<me::components::AudioSourceComponent>();
			}

			if (opened && !remove_component) {
				static me::components::AudioSourceComponent start_state;
				bool finished_editing = false;

				ImGui::Text("Audio Clip");
				ImGui::SameLine();
				std::string btn_text = audio->filepath.empty() ? "Drag .wav / .ogg Here" : std::filesystem::path(audio->filepath).filename().string();
				ImGui::Button(btn_text.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0));

				if (ImGui::BeginDragDropTarget()) {
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
						const char* dropped_path = (const char*)payload->Data;
						std::filesystem::path fp = dropped_path;
						if (fp.extension() == ".wav" || fp.extension() == ".ogg" || fp.extension() == ".mp3") {
							if (audio->clip.handle != 0) me::audio::release(audio->clip);
							audio->filepath = dropped_path;
							audio->clip = me::audio::load(dropped_path);
						}
					}
					ImGui::EndDragDropTarget();
				}

				ImGui::DragFloat("Volume", &audio->volume, 0.05f, 0.0f, 10.0f);
				if (ImGui::IsItemActivated()) start_state = *audio;
				if (ImGui::IsItemDeactivatedAfterEdit()) finished_editing = true;

				ImGui::DragFloat("Pitch", &audio->pitch, 0.05f, 0.1f, 3.0f);
				if (ImGui::IsItemActivated()) start_state = *audio;
				if (ImGui::IsItemDeactivatedAfterEdit()) finished_editing = true;

				ImGui::Checkbox("Play on Awake", &audio->play_on_awake);
				if (ImGui::IsItemActivated()) start_state = *audio;
				if (ImGui::IsItemDeactivatedAfterEdit()) finished_editing = true;

				ImGui::Separator();
				ImGui::Checkbox("Enable 3D Spatial Audio", &audio->spatial);
				if (ImGui::IsItemActivated()) start_state = *audio;
				if (ImGui::IsItemDeactivatedAfterEdit()) finished_editing = true;

				if (audio->spatial) {
					ImGui::DragFloat("Max Distance", &audio->max_distance, 1.0f, 0.1f, 1000.0f);
					if (ImGui::IsItemActivated()) start_state = *audio;
					if (ImGui::IsItemDeactivatedAfterEdit()) finished_editing = true;
				}

				ImGui::Separator();
				if (ImGui::Button("Test Play", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
					audio->trigger_play = true;
				}

				if (finished_editing) {
					auto cmd = std::make_unique<editor::ModifyComponentCommand<me::components::AudioSourceComponent>>(
						entity, start_state, *audio
					);
					command_history.AddCommand(std::move(cmd));
				}

				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	}

	void InspectorPanel::draw_audio_listener(me::Entity entity, editor::CommandHistory& command_history) {
		if (auto* listener = entity.try_get_component<me::components::AudioListenerComponent>()) {
			ImGui::PushID("AudioListener");

			float button_size = ImGui::GetFrameHeight();
			bool opened = ImGui::TreeNodeEx("Audio Listener", s_TreeNodeFlags);

			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - button_size);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
			bool remove_component = ImGui::Button("X", ImVec2(button_size, button_size));
			ImGui::PopStyleColor();

			if (remove_component) entity.remove_component<me::components::AudioListenerComponent>();

			if (opened && !remove_component) {
				static me::components::AudioListenerComponent start_state;
				bool finished_editing = false;

				ImGui::Checkbox("Active Listener", &listener->active);
				if (ImGui::IsItemActivated()) start_state = *listener;
				if (ImGui::IsItemDeactivatedAfterEdit()) finished_editing = true;

				if (finished_editing) {
					auto cmd = std::make_unique<editor::ModifyComponentCommand<me::components::AudioListenerComponent>>(
						entity, start_state, *listener
					);
					command_history.AddCommand(std::move(cmd));
				}

				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	}

	void InspectorPanel::draw_background_music(me::Entity entity, editor::CommandHistory& command_history) {
		if (auto* bgm = entity.try_get_component<me::components::BackgroundMusicComponent>()) {
			ImGui::PushID("BackgroundMusic");

			float button_size = ImGui::GetFrameHeight();
			bool opened = ImGui::TreeNodeEx("Background Music", s_TreeNodeFlags);

			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - button_size);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
			bool remove_component = ImGui::Button("X", ImVec2(button_size, button_size));
			ImGui::PopStyleColor();

			if (remove_component) {
				if (bgm->stream.handle != 0) me::audio::release(bgm->stream);
				entity.remove_component<me::components::BackgroundMusicComponent>();
			}

			if (opened && !remove_component) {
				static me::components::BackgroundMusicComponent start_state;
				bool finished_editing = false;

				ImGui::Text("Audio Track");
				ImGui::SameLine();
				std::string btn_text = bgm->filepath.empty() ? "Drag .wav / .ogg Here" : std::filesystem::path(bgm->filepath).filename().string();
				ImGui::Button(btn_text.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0));

				if (ImGui::BeginDragDropTarget()) {
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
						const char* dropped_path = (const char*)payload->Data;
						std::filesystem::path fp = dropped_path;
						if (fp.extension() == ".wav" || fp.extension() == ".ogg" || fp.extension() == ".mp3") {
							if (bgm->stream.handle != 0) me::audio::release(bgm->stream);
							bgm->filepath = dropped_path;
							bgm->stream = me::audio::load_music(dropped_path);
						}
					}
					ImGui::EndDragDropTarget();
				}

				ImGui::DragFloat("Volume", &bgm->volume, 0.05f, 0.0f, 10.0f);
				if (ImGui::IsItemActivated()) start_state = *bgm;
				if (ImGui::IsItemDeactivatedAfterEdit()) finished_editing = true;

				ImGui::Checkbox("Loop Track", &bgm->loop);
				if (ImGui::IsItemActivated()) start_state = *bgm;
				if (ImGui::IsItemDeactivatedAfterEdit()) finished_editing = true;

				ImGui::Checkbox("Play on Awake", &bgm->play_on_awake);
				if (ImGui::IsItemActivated()) start_state = *bgm;
				if (ImGui::IsItemDeactivatedAfterEdit()) finished_editing = true;

				ImGui::Separator();
				ImGui::TextDisabled("Playback Controls");

				if (ImGui::Button("Play", ImVec2(50, 0))) bgm->trigger_play = true;
				ImGui::SameLine();
				if (ImGui::Button("Pause", ImVec2(50, 0))) bgm->trigger_pause = true;
				ImGui::SameLine();
				if (ImGui::Button("Resume", ImVec2(60, 0))) bgm->trigger_resume = true;
				ImGui::SameLine();
				if (ImGui::Button("Stop", ImVec2(50, 0))) bgm->trigger_stop = true;

				if (finished_editing) {
					auto cmd = std::make_unique<editor::ModifyComponentCommand<me::components::BackgroundMusicComponent>>(
						entity, start_state, *bgm
					);
					command_history.AddCommand(std::move(cmd));
				}

				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	}

	void InspectorPanel::draw_script(me::Entity entity) {
		auto* script_comp = entity.try_get_component<me::components::ScriptComponent>();
		if (script_comp) {
			ImGui::PushID("Scripts");
			bool opened = ImGui::TreeNodeEx("Lua Scripts", s_TreeNodeFlags);

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
						entity.remove_component<me::components::ScriptComponent>();
						script_comp = nullptr;
					}
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}

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
						entity.add_component<me::components::ScriptComponent>(sc);
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
	}

	void InspectorPanel::draw_add_component_menu(me::Entity entity) {
		ImGui::Dummy(ImVec2(0, 10));
		ImGui::Separator();
		ImGui::Dummy(ImVec2(0, 10));

		if (ImGui::Button("Add Component", ImVec2(ImGui::GetContentRegionAvail().x, 30))) {
			ImGui::OpenPopup("AddComponentPopup");
		}

		if (ImGui::BeginPopup("AddComponentPopup")) {
			if (!entity.try_get_component<me::components::Shape3DComponent>() && ImGui::MenuItem("3D Primitive Shape"))
				entity.add_component<me::components::Shape3DComponent>(me::components::Shape3DComponent{ me::components::Shape3DComponent::Cube, me::Color::white });

			if (!entity.try_get_component<me::components::Model3DComponent>() && ImGui::MenuItem("3D Model"))
				entity.add_component<me::components::Model3DComponent>(me::components::Model3DComponent{ 0, me::Color::white });

			// --- ADD MATERIAL COMPONENT TO MENU ---
			if (!entity.try_get_component<me::components::MaterialComponent>() && ImGui::MenuItem("Material"))
				entity.add_component<me::components::MaterialComponent>(me::components::MaterialComponent{});

			if (!entity.try_get_component<me::components::CameraComponent>() && ImGui::MenuItem("Camera"))
				entity.add_component<me::components::CameraComponent>(me::components::CameraComponent{});

			if (!entity.try_get_component<me::components::LightComponent>() && ImGui::MenuItem("Light"))
				entity.add_component<me::components::LightComponent>(me::components::LightComponent{});

			if (!entity.try_get_component<me::components::DirectionalLightComponent>() && ImGui::MenuItem("Directional Light"))
				entity.add_component<me::components::DirectionalLightComponent>(me::components::DirectionalLightComponent{});

			ImGui::Separator();

			if (!entity.try_get_component<me::components::RigidBodyComponent>() && ImGui::MenuItem("Rigid Body"))
				entity.add_component<me::components::RigidBodyComponent>(me::components::RigidBodyComponent{});

			if (!entity.try_get_component<me::components::BoxColliderComponent>() && ImGui::MenuItem("Box Collider"))
				entity.add_component<me::components::BoxColliderComponent>(me::components::BoxColliderComponent{});

			if (!entity.try_get_component<me::components::SphereColliderComponent>() && ImGui::MenuItem("Sphere Collider"))
				entity.add_component<me::components::SphereColliderComponent>(me::components::SphereColliderComponent{});

			ImGui::Separator();

			if (!entity.try_get_component<me::components::AudioSourceComponent>() && ImGui::MenuItem("Audio Source"))
				entity.add_component<me::components::AudioSourceComponent>(me::components::AudioSourceComponent{});

			if (!entity.try_get_component<me::components::AudioListenerComponent>() && ImGui::MenuItem("Audio Listener"))
				entity.add_component<me::components::AudioListenerComponent>(me::components::AudioListenerComponent{});

			if (!entity.try_get_component<me::components::BackgroundMusicComponent>() && ImGui::MenuItem("Background Music"))
				entity.add_component<me::components::BackgroundMusicComponent>(me::components::BackgroundMusicComponent{});

			ImGui::EndPopup();
		}
	}

}