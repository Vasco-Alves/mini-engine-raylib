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

	void InspectorPanel::on_imgui_render(me::Registry* context, me::entity::entity_id selected_entity) {
		ImGui::Begin("Inspector");

		if (selected_entity != 0xFFFFFFFF && context) {
			draw_tag(context, selected_entity);
			draw_transform(context, selected_entity);
			draw_shape3d(context, selected_entity);
			draw_model3d(context, selected_entity);
			draw_light(context, selected_entity);
			draw_directional_light(context, selected_entity);
			draw_rigidbody(context, selected_entity);
			draw_box_collider(context, selected_entity);
			draw_sphere_collider(context, selected_entity);
			draw_audio_source(context, selected_entity);
			draw_audio_listener(context, selected_entity);
			draw_background_music(context, selected_entity);
			draw_script(context, selected_entity);

			draw_add_component_menu(context, selected_entity);
		} else {
			ImGui::Text("Select an entity to view its properties.");
		}

		ImGui::End();
	}

	void InspectorPanel::draw_tag(me::Registry* context, me::entity::entity_id entity) {
		if (auto* tag = context->try_get_component<me::components::TagComponent>(entity)) {
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
	}

	void InspectorPanel::draw_transform(me::Registry* context, me::entity::entity_id entity) {
		if (auto* transform = context->try_get_component<me::components::TransformComponent>(entity)) {
			if (ImGui::TreeNodeEx("Transform", s_TreeNodeFlags)) {
				ImGui::DragFloat3("Position", &transform->position.x, 0.1f);
				ImGui::DragFloat3("Rotation", &transform->rotation.x, 1.0f);
				ImGui::DragFloat3("Scale", &transform->scale.x, 0.1f);
				ImGui::TreePop();
			}
		}
	}

	void InspectorPanel::draw_shape3d(me::Registry* context, me::entity::entity_id entity) {
		if (auto* shape = context->try_get_component<me::components::Shape3DComponent>(entity)) {
			ImGui::PushID("Shape3D");

			float button_size = ImGui::GetFrameHeight();
			bool opened = ImGui::TreeNodeEx("Shape 3D", s_TreeNodeFlags);

			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - button_size);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
			bool remove_component = ImGui::Button("X", ImVec2(button_size, button_size));
			ImGui::PopStyleColor();

			if (remove_component) context->remove_component<me::components::Shape3DComponent>(entity);

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
	}

	void InspectorPanel::draw_model3d(me::Registry* context, me::entity::entity_id entity) {
		if (auto* modelComp = context->try_get_component<me::components::Model3DComponent>(entity)) {
			ImGui::PushID("Model3D");

			float button_size = ImGui::GetFrameHeight();
			bool opened = ImGui::TreeNodeEx("Model 3D", s_TreeNodeFlags);

			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - button_size);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
			bool remove_component = ImGui::Button("X", ImVec2(button_size, button_size));
			ImGui::PopStyleColor();

			if (remove_component) {
				if (modelComp->model.handle != 0) me::assets::release(modelComp->model);
				context->remove_component<me::components::Model3DComponent>(entity);
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
	}

	void InspectorPanel::draw_light(me::Registry* context, me::entity::entity_id entity) {
		if (auto* light = context->try_get_component<me::components::LightComponent>(entity)) {
			ImGui::PushID("Light");

			float button_size = ImGui::GetFrameHeight();
			bool opened = ImGui::TreeNodeEx("Light", s_TreeNodeFlags);

			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - button_size);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
			bool remove_component = ImGui::Button("X", ImVec2(button_size, button_size));
			ImGui::PopStyleColor();

			if (remove_component) context->remove_component<me::components::LightComponent>(entity);

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
	}

	void InspectorPanel::draw_directional_light(me::Registry* context, me::entity::entity_id entity) {
		if (auto* light = context->try_get_component<me::components::DirectionalLightComponent>(entity)) {
			ImGui::PushID("DirLight");

			float button_size = ImGui::GetFrameHeight();
			bool opened = ImGui::TreeNodeEx("Directional Light", s_TreeNodeFlags);

			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - button_size);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
			bool remove_component = ImGui::Button("X", ImVec2(button_size, button_size));
			ImGui::PopStyleColor();

			if (remove_component) context->remove_component<me::components::DirectionalLightComponent>(entity);

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
	}

	void InspectorPanel::draw_rigidbody(me::Registry* context, me::entity::entity_id entity) {
		if (auto* rb = context->try_get_component<me::components::RigidBodyComponent>(entity)) {
			ImGui::PushID("RigidBody");

			float button_size = ImGui::GetFrameHeight();
			bool opened = ImGui::TreeNodeEx("Rigid Body", s_TreeNodeFlags);

			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - button_size);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
			bool remove_component = ImGui::Button("X", ImVec2(button_size, button_size));
			ImGui::PopStyleColor();

			if (remove_component) context->remove_component<me::components::RigidBodyComponent>(entity);

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
	}

	void InspectorPanel::draw_box_collider(me::Registry* context, me::entity::entity_id entity) {
		if (auto* col = context->try_get_component<me::components::BoxColliderComponent>(entity)) {
			ImGui::PushID("BoxCollider");

			float button_size = ImGui::GetFrameHeight();
			bool opened = ImGui::TreeNodeEx("Box Collider", s_TreeNodeFlags);

			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - button_size);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
			bool remove_component = ImGui::Button("X", ImVec2(button_size, button_size));
			ImGui::PopStyleColor();

			if (remove_component) context->remove_component<me::components::BoxColliderComponent>(entity);

			if (opened) {
				if (!remove_component) {
					ImGui::DragFloat3("Half Extents", &col->half_extents.x, 0.1f, 0.01f, 100.0f);
					ImGui::Checkbox("Show Debug Wireframe", &col->show_debug);
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	}

	void InspectorPanel::draw_sphere_collider(me::Registry* context, me::entity::entity_id entity) {
		if (auto* col = context->try_get_component<me::components::SphereColliderComponent>(entity)) {
			ImGui::PushID("SphereCollider");

			float button_size = ImGui::GetFrameHeight();
			bool opened = ImGui::TreeNodeEx("Sphere Collider", s_TreeNodeFlags);

			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - button_size);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
			bool remove_component = ImGui::Button("X", ImVec2(button_size, button_size));
			ImGui::PopStyleColor();

			if (remove_component) context->remove_component<me::components::SphereColliderComponent>(entity);

			if (opened) {
				if (!remove_component) {
					ImGui::DragFloat("Radius", &col->radius, 0.1f, 0.01f, 100.0f);
					ImGui::Checkbox("Show Debug Wireframe", &col->show_debug);
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	}

	void InspectorPanel::draw_audio_source(me::Registry* context, me::entity::entity_id entity) {
		if (auto* audio = context->try_get_component<me::components::AudioSourceComponent>(entity)) {
			ImGui::PushID("AudioSource");

			float button_size = ImGui::GetFrameHeight();
			bool opened = ImGui::TreeNodeEx("Audio Source", s_TreeNodeFlags);

			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - button_size);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
			bool remove_component = ImGui::Button("X", ImVec2(button_size, button_size));
			ImGui::PopStyleColor();

			if (remove_component) {
				if (audio->clip.handle != 0) me::audio::release(audio->clip);
				context->remove_component<me::components::AudioSourceComponent>(entity);
			}

			if (opened) {
				if (!remove_component) {
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
					ImGui::DragFloat("Pitch", &audio->pitch, 0.05f, 0.1f, 3.0f);
					ImGui::Checkbox("Play on Awake", &audio->play_on_awake);

					ImGui::Separator();
					ImGui::Checkbox("Enable 3D Spatial Audio", &audio->spatial);
					if (audio->spatial) {
						ImGui::DragFloat("Max Distance", &audio->max_distance, 1.0f, 0.1f, 1000.0f);
					}

					ImGui::Separator();
					if (ImGui::Button("Test Play", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
						audio->trigger_play = true;
					}
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	}

	void InspectorPanel::draw_audio_listener(me::Registry* context, me::entity::entity_id entity) {
		if (auto* listener = context->try_get_component<me::components::AudioListenerComponent>(entity)) {
			ImGui::PushID("AudioListener");

			float button_size = ImGui::GetFrameHeight();
			bool opened = ImGui::TreeNodeEx("Audio Listener", s_TreeNodeFlags);

			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - button_size);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
			bool remove_component = ImGui::Button("X", ImVec2(button_size, button_size));
			ImGui::PopStyleColor();

			if (remove_component) context->remove_component<me::components::AudioListenerComponent>(entity);

			if (opened) {
				if (!remove_component) {
					ImGui::Checkbox("Active Listener", &listener->active);
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	}

	void InspectorPanel::draw_background_music(me::Registry* context, me::entity::entity_id entity) {
		if (auto* bgm = context->try_get_component<me::components::BackgroundMusicComponent>(entity)) {
			ImGui::PushID("BackgroundMusic");

			float button_size = ImGui::GetFrameHeight();
			bool opened = ImGui::TreeNodeEx("Background Music", s_TreeNodeFlags);

			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - button_size);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
			bool remove_component = ImGui::Button("X", ImVec2(button_size, button_size));
			ImGui::PopStyleColor();

			if (remove_component) {
				if (bgm->stream.handle != 0) me::audio::release(bgm->stream);
				context->remove_component<me::components::BackgroundMusicComponent>(entity);
			}

			if (opened) {
				if (!remove_component) {
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
					ImGui::Checkbox("Loop Track", &bgm->loop);
					ImGui::Checkbox("Play on Awake", &bgm->play_on_awake);

					ImGui::Separator();
					ImGui::TextDisabled("Playback Controls");

					if (ImGui::Button("Play", ImVec2(50, 0))) bgm->trigger_play = true;
					ImGui::SameLine();
					if (ImGui::Button("Pause", ImVec2(50, 0))) bgm->trigger_pause = true;
					ImGui::SameLine();
					if (ImGui::Button("Resume", ImVec2(60, 0))) bgm->trigger_resume = true;
					ImGui::SameLine();
					if (ImGui::Button("Stop", ImVec2(50, 0))) bgm->trigger_stop = true;
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	}

	void InspectorPanel::draw_script(me::Registry* context, me::entity::entity_id entity) {
		auto* script_comp = context->try_get_component<me::components::ScriptComponent>(entity);
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
						context->remove_component<me::components::ScriptComponent>(entity);
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
						context->add_component<me::components::ScriptComponent>(entity, sc);
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

	void InspectorPanel::draw_add_component_menu(me::Registry* context, me::entity::entity_id entity) {
		ImGui::Dummy(ImVec2(0, 10));
		ImGui::Separator();
		ImGui::Dummy(ImVec2(0, 10));

		if (ImGui::Button("Add Component", ImVec2(ImGui::GetContentRegionAvail().x, 30))) {
			ImGui::OpenPopup("AddComponentPopup");
		}

		if (ImGui::BeginPopup("AddComponentPopup")) {
			if (!context->try_get_component<me::components::Shape3DComponent>(entity) && ImGui::MenuItem("3D Primitive Shape"))
				context->add_component<me::components::Shape3DComponent>(entity, me::components::Shape3DComponent{ me::components::Shape3DComponent::Cube, me::Color::white });

			if (!context->try_get_component<me::components::Model3DComponent>(entity) && ImGui::MenuItem("3D Model"))
				context->add_component<me::components::Model3DComponent>(entity, me::components::Model3DComponent{ 0, me::Color::white });

			if (!context->try_get_component<me::components::CameraComponent>(entity) && ImGui::MenuItem("Camera"))
				context->add_component<me::components::CameraComponent>(entity, me::components::CameraComponent{});

			if (!context->try_get_component<me::components::LightComponent>(entity) && ImGui::MenuItem("Light"))
				context->add_component<me::components::LightComponent>(entity, me::components::LightComponent{});

			if (!context->try_get_component<me::components::DirectionalLightComponent>(entity) && ImGui::MenuItem("Directional Light"))
				context->add_component<me::components::DirectionalLightComponent>(entity, me::components::DirectionalLightComponent{});

			ImGui::Separator();

			if (!context->try_get_component<me::components::RigidBodyComponent>(entity) && ImGui::MenuItem("Rigid Body"))
				context->add_component<me::components::RigidBodyComponent>(entity, me::components::RigidBodyComponent{});

			if (!context->try_get_component<me::components::BoxColliderComponent>(entity) && ImGui::MenuItem("Box Collider"))
				context->add_component<me::components::BoxColliderComponent>(entity, me::components::BoxColliderComponent{});

			if (!context->try_get_component<me::components::SphereColliderComponent>(entity) && ImGui::MenuItem("Sphere Collider"))
				context->add_component<me::components::SphereColliderComponent>(entity, me::components::SphereColliderComponent{});

			ImGui::Separator();

			if (!context->try_get_component<me::components::AudioSourceComponent>(entity) && ImGui::MenuItem("Audio Source"))
				context->add_component<me::components::AudioSourceComponent>(entity, me::components::AudioSourceComponent{});

			if (!context->try_get_component<me::components::AudioListenerComponent>(entity) && ImGui::MenuItem("Audio Listener"))
				context->add_component<me::components::AudioListenerComponent>(entity, me::components::AudioListenerComponent{});

			if (!context->try_get_component<me::components::BackgroundMusicComponent>(entity) && ImGui::MenuItem("Background Music"))
				context->add_component<me::components::BackgroundMusicComponent>(entity, me::components::BackgroundMusicComponent{});

			ImGui::EndPopup();
		}
	}

}