#include "editor/panels/inspector_panel.hpp"

#include <mini-engine-raylib/ecs/components.hpp>
#include <mini-engine-raylib/ecs/physics_components.hpp>
#include <mini-engine-raylib/ecs/audio_components.hpp>
#include <mini-engine-raylib/ecs/script_component.hpp>
#include <mini-engine-raylib/assets/assets.hpp>
#include <mini-engine-raylib/audio/audio.hpp>
#include <mini-engine-raylib/core/logger.hpp>
#include <mini-engine-raylib/render/color.hpp>
#include <imgui.h>
#include <cstdint>
#include <memory>
#include <string>
#include <filesystem>

namespace editor {

	namespace {

		const ImGuiTreeNodeFlags s_TreeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_SpanAvailWidth;

		// --- Color conversion between me::Color (0-255) and ImGui's float[4] (0-1) ---
		inline void color_to_floats(const me::Color& c, float out[4]) {
			out[0] = c.r / 255.0f;
			out[1] = c.g / 255.0f;
			out[2] = c.b / 255.0f;
			out[3] = c.a / 255.0f;
		}

		inline me::Color color_from_floats(const float in[4]) {
			return me::Color{
				static_cast<std::uint8_t>(in[0] * 255.0f),
				static_cast<std::uint8_t>(in[1] * 255.0f),
				static_cast<std::uint8_t>(in[2] * 255.0f),
				static_cast<std::uint8_t>(in[3] * 255.0f)
			};
		}

		// RAII collapsible component header with a red [X] remove button.
		// The destructor always balances the ImGui tree/ID stack, so removing a
		// component while its node is open can't leave an unmatched TreePush.
		class ComponentSection {
		public:
			ComponentSection(const char* id, const char* label) {
				ImGui::PushID(id);
				m_Opened = ImGui::TreeNodeEx(label, s_TreeNodeFlags);

				float button_size = ImGui::GetFrameHeight();
				ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - button_size);
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
				m_Remove = ImGui::Button("X", ImVec2(button_size, button_size));
				ImGui::PopStyleColor();
			}

			~ComponentSection() {
				if (m_Opened) ImGui::TreePop();
				ImGui::PopID();
			}

			ComponentSection(const ComponentSection&) = delete;
			ComponentSection& operator=(const ComponentSection&) = delete;

			bool remove_clicked() const { return m_Remove; }
			// True when the node's body should be drawn (open and not being removed).
			bool body_visible() const { return m_Opened && !m_Remove; }

		private:
			bool m_Opened = false;
			bool m_Remove = false;
		};

		// Records an Undo command when an edit on the preceding widget completes.
		// Call immediately after the widget. One shared snapshot per component type
		// is sufficient: only a single widget can be active at any moment.
		template <typename T>
		void track_edit(me::Entity entity, T* comp, editor::CommandHistory& history) {
			static T s_start{};
			if (ImGui::IsItemActivated()) s_start = *comp;
			if (ImGui::IsItemDeactivatedAfterEdit()) {
				history.AddCommand(std::make_unique<editor::ModifyComponentCommand<T>>(entity, s_start, *comp));
			}
		}

	} // namespace

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
		auto* tag = entity.try_get_component<me::components::TagComponent>();
		if (!tag) return;

		char buffer[256];
		memset(buffer, 0, sizeof(buffer));
		strncpy(buffer, tag->name.c_str(), sizeof(buffer) - 1);

		ImGui::Text("Name");
		ImGui::SameLine();
		ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);

		if (ImGui::InputText("##Tag", buffer, sizeof(buffer))) {
			tag->name = std::string(buffer);
		}
		track_edit(entity, tag, command_history);

		ImGui::PopItemWidth();
		ImGui::Dummy(ImVec2(0, 10));
	}

	void InspectorPanel::draw_transform(me::Entity entity, editor::CommandHistory& command_history) {
		auto* transform = entity.try_get_component<me::components::TransformComponent>();
		if (!transform) return;

		// Transform has no remove button, so it keeps its own (always-balanced) tree node.
		if (ImGui::TreeNodeEx("Transform", s_TreeNodeFlags)) {
			ImGui::DragFloat3("Position", &transform->position.x, 0.1f);
			track_edit(entity, transform, command_history);

			ImGui::DragFloat3("Rotation", &transform->rotation.x, 1.0f);
			track_edit(entity, transform, command_history);

			ImGui::DragFloat3("Scale", &transform->scale.x, 0.1f);
			track_edit(entity, transform, command_history);

			ImGui::TreePop();
		}
	}

	void InspectorPanel::draw_shape3d(me::Entity entity, editor::CommandHistory& command_history) {
		auto* shape = entity.try_get_component<me::components::Shape3DComponent>();
		if (!shape) return;

		ComponentSection section("Shape3D", "Shape 3D");
		if (section.remove_clicked()) { entity.remove_component<me::components::Shape3DComponent>(); return; }
		if (!section.body_visible()) return;

		const char* types[] = { "Cube", "Sphere", "Plane" };
		int current_type = static_cast<int>(shape->type);
		if (ImGui::Combo("Primitive", &current_type, types, 3)) {
			shape->type = static_cast<me::components::Shape3DComponent::Type>(current_type);
		}
		track_edit(entity, shape, command_history);

		float color[4];
		color_to_floats(shape->color, color);
		if (ImGui::ColorEdit4("Color", color)) shape->color = color_from_floats(color);
		track_edit(entity, shape, command_history);

		ImGui::Checkbox("Wireframe", &shape->wireframe);
		track_edit(entity, shape, command_history);
	}

	void InspectorPanel::draw_material(me::Entity entity, editor::CommandHistory& command_history) {
		auto* mat = entity.try_get_component<me::components::MaterialComponent>();
		if (!mat) return;

		ComponentSection section("Material", "Material");
		if (section.remove_clicked()) { entity.remove_component<me::components::MaterialComponent>(); return; }
		if (!section.body_visible()) return;

		float color[4];
		color_to_floats(mat->albedo, color);
		if (ImGui::ColorEdit4("Albedo", color)) mat->albedo = color_from_floats(color);
		track_edit(entity, mat, command_history);

		ImGui::SliderFloat("Roughness", &mat->roughness, 0.0f, 1.0f);
		track_edit(entity, mat, command_history);

		ImGui::SliderFloat("Metallic", &mat->metallic, 0.0f, 1.0f);
		track_edit(entity, mat, command_history);

		ImGui::DragFloat("Emission Power", &mat->emission_power, 0.1f, 0.0f, 100.0f);
		track_edit(entity, mat, command_history);

		ImGui::SliderFloat("Transmission (Glass)", &mat->transmission, 0.0f, 1.0f);
		track_edit(entity, mat, command_history);

		ImGui::DragFloat("IOR", &mat->ior, 0.01f, 1.0f, 3.0f, "%.2f");
		track_edit(entity, mat, command_history);
	}

	void InspectorPanel::draw_model3d(me::Entity entity, editor::CommandHistory& command_history) {
		auto* modelComp = entity.try_get_component<me::components::Model3DComponent>();
		if (!modelComp) return;

		ComponentSection section("Model3D", "Model 3D");
		if (section.remove_clicked()) {
			if (modelComp->model.handle != 0) me::assets::release(modelComp->model);
			entity.remove_component<me::components::Model3DComponent>();
			return;
		}
		if (!section.body_visible()) return;

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

	void InspectorPanel::draw_light(me::Entity entity, editor::CommandHistory& command_history) {
		auto* light = entity.try_get_component<me::components::LightComponent>();
		if (!light) return;

		ComponentSection section("Light", "Light");
		if (section.remove_clicked()) { entity.remove_component<me::components::LightComponent>(); return; }
		if (!section.body_visible()) return;

		float color[4];
		color_to_floats(light->color, color);
		if (ImGui::ColorEdit3("Color", color)) light->color = color_from_floats(color);
		track_edit(entity, light, command_history);

		ImGui::DragFloat("Intensity", &light->intensity, 0.1f, 0.0f, 100.0f);
		track_edit(entity, light, command_history);
	}

	void InspectorPanel::draw_directional_light(me::Entity entity, editor::CommandHistory& command_history) {
		auto* light = entity.try_get_component<me::components::DirectionalLightComponent>();
		if (!light) return;

		ComponentSection section("DirLight", "Directional Light");
		if (section.remove_clicked()) { entity.remove_component<me::components::DirectionalLightComponent>(); return; }
		if (!section.body_visible()) return;

		float color[4];
		color_to_floats(light->color, color);
		if (ImGui::ColorEdit3("Color", color)) light->color = color_from_floats(color);
		track_edit(entity, light, command_history);

		ImGui::DragFloat("Intensity", &light->intensity, 0.1f, 0.0f, 100.0f);
		track_edit(entity, light, command_history);
	}

	void InspectorPanel::draw_rigidbody(me::Entity entity, editor::CommandHistory& command_history) {
		auto* rb = entity.try_get_component<me::components::RigidBodyComponent>();
		if (!rb) return;

		ComponentSection section("RigidBody", "Rigid Body");
		if (section.remove_clicked()) { entity.remove_component<me::components::RigidBodyComponent>(); return; }
		if (!section.body_visible()) return;

		const char* body_types[] = { "Static", "Dynamic", "Kinematic" };
		int current_type = static_cast<int>(rb->type);
		if (ImGui::Combo("Body Type", &current_type, body_types, 3)) {
			rb->type = static_cast<me::components::RigidBodyType>(current_type);
		}
		track_edit(entity, rb, command_history);

		if (rb->type == me::components::RigidBodyType::Dynamic) {
			ImGui::DragFloat("Mass", &rb->mass, 0.1f, 0.001f, 1000.0f);
			track_edit(entity, rb, command_history);
		}

		ImGui::DragFloat("Bounciness", &rb->bounciness, 0.05f, 0.0f, 1.0f);
		track_edit(entity, rb, command_history);

		ImGui::DragFloat("Friction", &rb->friction, 0.05f, 0.0f, 10.0f);
		track_edit(entity, rb, command_history);
	}

	void InspectorPanel::draw_box_collider(me::Entity entity, editor::CommandHistory& command_history) {
		auto* col = entity.try_get_component<me::components::BoxColliderComponent>();
		if (!col) return;

		ComponentSection section("BoxCollider", "Box Collider");
		if (section.remove_clicked()) { entity.remove_component<me::components::BoxColliderComponent>(); return; }
		if (!section.body_visible()) return;

		ImGui::DragFloat3("Half Extents", &col->half_extents.x, 0.1f, 0.01f, 100.0f);
		track_edit(entity, col, command_history);

		ImGui::Checkbox("Show Debug Wireframe", &col->show_debug);
		track_edit(entity, col, command_history);
	}

	void InspectorPanel::draw_sphere_collider(me::Entity entity, editor::CommandHistory& command_history) {
		auto* col = entity.try_get_component<me::components::SphereColliderComponent>();
		if (!col) return;

		ComponentSection section("SphereCollider", "Sphere Collider");
		if (section.remove_clicked()) { entity.remove_component<me::components::SphereColliderComponent>(); return; }
		if (!section.body_visible()) return;

		ImGui::DragFloat("Radius", &col->radius, 0.1f, 0.01f, 100.0f);
		track_edit(entity, col, command_history);

		ImGui::Checkbox("Show Debug Wireframe", &col->show_debug);
		track_edit(entity, col, command_history);
	}

	void InspectorPanel::draw_audio_source(me::Entity entity, editor::CommandHistory& command_history) {
		auto* audio = entity.try_get_component<me::components::AudioSourceComponent>();
		if (!audio) return;

		ComponentSection section("AudioSource", "Audio Source");
		if (section.remove_clicked()) {
			if (audio->clip.handle != 0) me::audio::release(audio->clip);
			entity.remove_component<me::components::AudioSourceComponent>();
			return;
		}
		if (!section.body_visible()) return;

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
		track_edit(entity, audio, command_history);

		ImGui::DragFloat("Pitch", &audio->pitch, 0.05f, 0.1f, 3.0f);
		track_edit(entity, audio, command_history);

		ImGui::Checkbox("Play on Awake", &audio->play_on_awake);
		track_edit(entity, audio, command_history);

		ImGui::Separator();
		ImGui::Checkbox("Enable 3D Spatial Audio", &audio->spatial);
		track_edit(entity, audio, command_history);

		if (audio->spatial) {
			ImGui::DragFloat("Max Distance", &audio->max_distance, 1.0f, 0.1f, 1000.0f);
			track_edit(entity, audio, command_history);
		}

		ImGui::Separator();
		if (ImGui::Button("Test Play", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
			audio->trigger_play = true;
		}
	}

	void InspectorPanel::draw_audio_listener(me::Entity entity, editor::CommandHistory& command_history) {
		auto* listener = entity.try_get_component<me::components::AudioListenerComponent>();
		if (!listener) return;

		ComponentSection section("AudioListener", "Audio Listener");
		if (section.remove_clicked()) { entity.remove_component<me::components::AudioListenerComponent>(); return; }
		if (!section.body_visible()) return;

		ImGui::Checkbox("Active Listener", &listener->active);
		track_edit(entity, listener, command_history);
	}

	void InspectorPanel::draw_background_music(me::Entity entity, editor::CommandHistory& command_history) {
		auto* bgm = entity.try_get_component<me::components::BackgroundMusicComponent>();
		if (!bgm) return;

		ComponentSection section("BackgroundMusic", "Background Music");
		if (section.remove_clicked()) {
			if (bgm->stream.handle != 0) me::audio::release(bgm->stream);
			entity.remove_component<me::components::BackgroundMusicComponent>();
			return;
		}
		if (!section.body_visible()) return;

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
		track_edit(entity, bgm, command_history);

		ImGui::Checkbox("Loop Track", &bgm->loop);
		track_edit(entity, bgm, command_history);

		ImGui::Checkbox("Play on Awake", &bgm->play_on_awake);
		track_edit(entity, bgm, command_history);

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
