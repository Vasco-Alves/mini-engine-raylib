#include "editor/panels/inspector_panel.hpp"
#include "editor/core/entity_commands.hpp"

#include <mini-engine-raylib/core/engine.hpp>
#include <mini-engine-raylib/ecs/components.hpp>
#include <mini-engine-raylib/ecs/physics_components.hpp>
#include <mini-engine-raylib/ecs/audio_components.hpp>
#include <mini-engine-raylib/ecs/script_component.hpp>
#include <mini-engine-raylib/assets/assets.hpp>
#include <mini-engine-raylib/audio/audio.hpp>
#include <mini-engine-raylib/core/logger.hpp>
#include <mini-engine-raylib/render/color.hpp>
#include <imgui.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>
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

		// Drag-widget speed: hold Alt for 10x finer control. (ImGui also supports
		// Ctrl+Click on any drag/slider to type an exact value.)
		inline float fine(float v_speed) {
			return ImGui::GetIO().KeyAlt ? v_speed * 0.1f : v_speed;
		}

		// Removes a component through the undo history. The command snapshots the
		// component (asset paths included), so Ctrl+Z restores it intact, and the
		// registry releases any native handle the component owns.
		void remove_via_history(me::Entity entity, const char* meta_name, editor::CommandHistory& history) {
			history.AddCommand(std::make_unique<editor::RemoveComponentCommand>(
				me::get_registry(), entity.get_id(), meta_name));
		}

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

		// =================================================================
		// PER-COMPONENT PANELS
		// =================================================================

		void draw_tag(me::Entity entity, editor::CommandHistory& command_history) {
			auto* tag = entity.try_get_component<me::components::TagComponent>();
			if (!tag) return;

			char buffer[256];
			snprintf(buffer, sizeof(buffer), "%s", tag->name.c_str());

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

		void draw_transform(me::Entity entity, editor::CommandHistory& command_history) {
			auto* transform = entity.try_get_component<me::components::TransformComponent>();
			if (!transform) return;

			// Transform has no remove button, so it keeps its own (always-balanced) tree node.
			if (ImGui::TreeNodeEx("Transform", s_TreeNodeFlags)) {
				ImGui::DragFloat3("Position", &transform->position.x, fine(0.1f));
				track_edit(entity, transform, command_history);

				ImGui::DragFloat3("Rotation", &transform->rotation.x, fine(1.0f));
				track_edit(entity, transform, command_history);

				ImGui::DragFloat3("Scale", &transform->scale.x, fine(0.1f));
				track_edit(entity, transform, command_history);

				ImGui::TreePop();
			}
		}

		void draw_shape3d(me::Entity entity, editor::CommandHistory& command_history) {
			auto* shape = entity.try_get_component<me::components::Shape3DComponent>();
			if (!shape) return;

			ComponentSection section("Shape3D", "Shape 3D");
			if (section.remove_clicked()) { remove_via_history(entity, "MeshRenderer", command_history); return; }
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

		void draw_material(me::Entity entity, editor::CommandHistory& command_history) {
			auto* mat = entity.try_get_component<me::components::MaterialComponent>();
			if (!mat) return;

			ComponentSection section("Material", "Material");
			if (section.remove_clicked()) { remove_via_history(entity, "Material", command_history); return; }
			if (!section.body_visible()) return;

			float color[4];
			color_to_floats(mat->albedo, color);
			if (ImGui::ColorEdit4("Albedo", color)) mat->albedo = color_from_floats(color);
			track_edit(entity, mat, command_history);

			ImGui::SliderFloat("Roughness", &mat->roughness, 0.0f, 1.0f);
			track_edit(entity, mat, command_history);

			ImGui::SliderFloat("Metallic", &mat->metallic, 0.0f, 1.0f);
			track_edit(entity, mat, command_history);

			ImGui::DragFloat("Emission Power", &mat->emission_power, fine(0.1f), 0.0f, 100.0f);
			track_edit(entity, mat, command_history);

			ImGui::SliderFloat("Transmission (Glass)", &mat->transmission, 0.0f, 1.0f);
			track_edit(entity, mat, command_history);

			ImGui::DragFloat("IOR", &mat->ior, fine(0.01f), 1.0f, 3.0f, "%.2f");
			track_edit(entity, mat, command_history);

			// How strongly the glass interior absorbs toward the albedo:
			// 0 = always clear, 1 = physical Beer–Lambert, higher = denser tint.
			ImGui::SliderFloat("Tint Strength", &mat->tint_strength, 0.0f, 10.0f, "%.2f");
			track_edit(entity, mat, command_history);
		}

		void draw_model3d(me::Entity entity, editor::CommandHistory& command_history) {
			auto* modelComp = entity.try_get_component<me::components::Model3DComponent>();
			if (!modelComp) return;

			ComponentSection section("Model3D", "Model 3D");
			if (section.remove_clicked()) { remove_via_history(entity, "Model", command_history); return; }
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

		void draw_light(me::Entity entity, editor::CommandHistory& command_history) {
			auto* light = entity.try_get_component<me::components::LightComponent>();
			if (!light) return;

			ComponentSection section("Light", "Light");
			if (section.remove_clicked()) { remove_via_history(entity, "Light", command_history); return; }
			if (!section.body_visible()) return;

			float color[4];
			color_to_floats(light->color, color);
			if (ImGui::ColorEdit3("Color", color)) light->color = color_from_floats(color);
			track_edit(entity, light, command_history);

			ImGui::DragFloat("Intensity", &light->intensity, fine(0.1f), 0.0f, 100.0f);
			track_edit(entity, light, command_history);

			ImGui::DragFloat("Radius (Softness)", &light->radius, fine(0.01f), 0.0f, 20.0f);
			track_edit(entity, light, command_history);
		}

		void draw_directional_light(me::Entity entity, editor::CommandHistory& command_history) {
			auto* light = entity.try_get_component<me::components::DirectionalLightComponent>();
			if (!light) return;

			ComponentSection section("DirLight", "Directional Light");
			if (section.remove_clicked()) { remove_via_history(entity, "DirectionalLight", command_history); return; }
			if (!section.body_visible()) return;

			float color[4];
			color_to_floats(light->color, color);
			if (ImGui::ColorEdit3("Color", color)) light->color = color_from_floats(color);
			track_edit(entity, light, command_history);

			ImGui::DragFloat("Intensity", &light->intensity, fine(0.1f), 0.0f, 100.0f);
			track_edit(entity, light, command_history);

			ImGui::DragFloat("Angular Radius (deg)", &light->angular_radius, fine(0.05f), 0.0f, 30.0f);
			track_edit(entity, light, command_history);
		}

		void draw_camera(me::Entity entity, editor::CommandHistory& command_history) {
			auto* cam = entity.try_get_component<me::components::CameraComponent>();
			if (!cam) return;

			ComponentSection section("Camera", "Camera");
			if (section.remove_clicked()) { remove_via_history(entity, "Camera", command_history); return; }
			if (!section.body_visible()) return;

			ImGui::Checkbox("Active (primary)", &cam->active);
			track_edit(entity, cam, command_history);

			const char* projections[] = { "Perspective", "Orthographic" };
			int proj = cam->projection;
			if (ImGui::Combo("Projection", &proj, projections, 2)) cam->projection = proj;
			track_edit(entity, cam, command_history);

			ImGui::DragFloat("Field of View", &cam->fov, fine(0.5f), 1.0f, 179.0f);
			track_edit(entity, cam, command_history);

			ImGui::DragFloat3("Look-at Target", &cam->target.x, fine(0.1f));
			track_edit(entity, cam, command_history);

			ImGui::DragFloat3("Up", &cam->up.x, fine(0.05f));
			track_edit(entity, cam, command_history);
		}

		void draw_rigidbody(me::Entity entity, editor::CommandHistory& command_history) {
			auto* rb = entity.try_get_component<me::components::RigidBodyComponent>();
			if (!rb) return;

			ComponentSection section("RigidBody", "Rigid Body");
			if (section.remove_clicked()) { remove_via_history(entity, "RigidBody", command_history); return; }
			if (!section.body_visible()) return;

			const char* body_types[] = { "Static", "Dynamic", "Kinematic" };
			int current_type = static_cast<int>(rb->type);
			if (ImGui::Combo("Body Type", &current_type, body_types, 3)) {
				rb->type = static_cast<me::components::RigidBodyType>(current_type);
			}
			track_edit(entity, rb, command_history);

			if (rb->type == me::components::RigidBodyType::Dynamic) {
				ImGui::DragFloat("Mass", &rb->mass, fine(0.1f), 0.001f, 1000.0f);
				track_edit(entity, rb, command_history);
			}

			ImGui::DragFloat("Bounciness", &rb->bounciness, fine(0.05f), 0.0f, 1.0f);
			track_edit(entity, rb, command_history);

			ImGui::DragFloat("Friction", &rb->friction, fine(0.05f), 0.0f, 10.0f);
			track_edit(entity, rb, command_history);
		}

		void draw_box_collider(me::Entity entity, editor::CommandHistory& command_history) {
			auto* col = entity.try_get_component<me::components::BoxColliderComponent>();
			if (!col) return;

			ComponentSection section("BoxCollider", "Box Collider");
			if (section.remove_clicked()) { remove_via_history(entity, "BoxCollider", command_history); return; }
			if (!section.body_visible()) return;

			ImGui::DragFloat3("Half Extents", &col->half_extents.x, fine(0.1f), 0.01f, 100.0f);
			track_edit(entity, col, command_history);

			ImGui::Checkbox("Show Debug Wireframe", &col->show_debug);
			track_edit(entity, col, command_history);
		}

		void draw_sphere_collider(me::Entity entity, editor::CommandHistory& command_history) {
			auto* col = entity.try_get_component<me::components::SphereColliderComponent>();
			if (!col) return;

			ComponentSection section("SphereCollider", "Sphere Collider");
			if (section.remove_clicked()) { remove_via_history(entity, "SphereCollider", command_history); return; }
			if (!section.body_visible()) return;

			ImGui::DragFloat("Radius", &col->radius, fine(0.1f), 0.01f, 100.0f);
			track_edit(entity, col, command_history);

			ImGui::Checkbox("Show Debug Wireframe", &col->show_debug);
			track_edit(entity, col, command_history);
		}

		void draw_audio_source(me::Entity entity, editor::CommandHistory& command_history) {
			auto* audio = entity.try_get_component<me::components::AudioSourceComponent>();
			if (!audio) return;

			ComponentSection section("AudioSource", "Audio Source");
			if (section.remove_clicked()) { remove_via_history(entity, "AudioSource", command_history); return; }
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

			ImGui::DragFloat("Volume", &audio->volume, fine(0.05f), 0.0f, 10.0f);
			track_edit(entity, audio, command_history);

			ImGui::DragFloat("Pitch", &audio->pitch, fine(0.05f), 0.1f, 3.0f);
			track_edit(entity, audio, command_history);

			ImGui::Checkbox("Play on Awake", &audio->play_on_awake);
			track_edit(entity, audio, command_history);

			ImGui::Separator();
			ImGui::Checkbox("Enable 3D Spatial Audio", &audio->spatial);
			track_edit(entity, audio, command_history);

			if (audio->spatial) {
				ImGui::DragFloat("Max Distance", &audio->max_distance, fine(1.0f), 0.1f, 1000.0f);
				track_edit(entity, audio, command_history);
			}

			ImGui::Separator();
			if (ImGui::Button("Test Play", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
				audio->trigger_play = true;
			}
		}

		void draw_audio_listener(me::Entity entity, editor::CommandHistory& command_history) {
			auto* listener = entity.try_get_component<me::components::AudioListenerComponent>();
			if (!listener) return;

			ComponentSection section("AudioListener", "Audio Listener");
			if (section.remove_clicked()) { remove_via_history(entity, "AudioListener", command_history); return; }
			if (!section.body_visible()) return;

			ImGui::Checkbox("Active Listener", &listener->active);
			track_edit(entity, listener, command_history);
		}

		void draw_background_music(me::Entity entity, editor::CommandHistory& command_history) {
			auto* bgm = entity.try_get_component<me::components::BackgroundMusicComponent>();
			if (!bgm) return;

			ComponentSection section("BackgroundMusic", "Background Music");
			if (section.remove_clicked()) { remove_via_history(entity, "BackgroundMusic", command_history); return; }
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

			ImGui::DragFloat("Volume", &bgm->volume, fine(0.05f), 0.0f, 10.0f);
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

		void draw_script(me::Entity entity, editor::CommandHistory& command_history) {
			auto* script_comp = entity.try_get_component<me::components::ScriptComponent>();

			// Script attach/detach goes through the undo history as a whole-
			// component snapshot; the registry re-creates instances from paths.
			auto current_paths = [&]() {
				std::vector<std::string> p;
				if (auto* sc = entity.try_get_component<me::components::ScriptComponent>())
					for (const auto& inst : sc->scripts) p.push_back(inst.path);
				return p;
				};
			auto commit_scripts = [&](const std::vector<std::string>& paths) {
				me::ecs::json state = nullptr;
				if (!paths.empty()) {
					me::ecs::json arr = me::ecs::json::array();
					for (const auto& p : paths) arr.push_back({ {"path", p} });
					state = me::ecs::json{ {"scripts", arr} };
				}
				command_history.AddCommand(std::make_unique<editor::ReplaceComponentCommand>(
					me::get_registry(), entity.get_id(), "Script", std::move(state)));
				};

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
						auto paths = current_paths();
						if (script_to_delete < paths.size()) {
							paths.erase(paths.begin() + script_to_delete);
							commit_scripts(paths); // empty list removes the component
						}
						script_comp = entity.try_get_component<me::components::ScriptComponent>();
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
						auto paths = current_paths();
						bool already_attached = std::find(paths.begin(), paths.end(), path_str) != paths.end();
						if (!already_attached) {
							paths.push_back(path_str);
							commit_scripts(paths);
						}
					}
				}
				ImGui::EndDragDropTarget();
			}
		}

		// =================================================================
		// COMPONENT TABLE — single source of truth for the per-component panels
		// (driven by on_imgui_render) and the Add-Component menu.
		// =================================================================

		template <typename T> bool insp_has(me::Entity e) { return e.has_component<T>(); }

		enum class AddGroup { Scene, Physics, Audio };

		struct InspectorComponent {
			const char* label;                                   // Add-menu text
			AddGroup group;                                      // Add-menu separators
			bool (*has)(me::Entity);
			void (*draw)(me::Entity, editor::CommandHistory&);   // nullptr => addable but no panel yet
			const char* meta_name;                               // me::ecs registry key (drives undoable add)
		};

		const std::vector<InspectorComponent>& inspector_components() {
			using namespace me::components;
			static const std::vector<InspectorComponent> table = {
				{ "3D Primitive Shape", AddGroup::Scene,   insp_has<Shape3DComponent>,          draw_shape3d,            "MeshRenderer" },
				{ "3D Model",           AddGroup::Scene,   insp_has<Model3DComponent>,          draw_model3d,            "Model" },
				{ "Material",           AddGroup::Scene,   insp_has<MaterialComponent>,         draw_material,           "Material" },
				{ "Camera",             AddGroup::Scene,   insp_has<CameraComponent>,           draw_camera,             "Camera" },
				{ "Light",              AddGroup::Scene,   insp_has<LightComponent>,            draw_light,              "Light" },
				{ "Directional Light",  AddGroup::Scene,   insp_has<DirectionalLightComponent>, draw_directional_light,  "DirectionalLight" },
				{ "Rigid Body",         AddGroup::Physics, insp_has<RigidBodyComponent>,        draw_rigidbody,          "RigidBody" },
				{ "Box Collider",       AddGroup::Physics, insp_has<BoxColliderComponent>,      draw_box_collider,       "BoxCollider" },
				{ "Sphere Collider",    AddGroup::Physics, insp_has<SphereColliderComponent>,   draw_sphere_collider,    "SphereCollider" },
				{ "Audio Source",       AddGroup::Audio,   insp_has<AudioSourceComponent>,      draw_audio_source,       "AudioSource" },
				{ "Audio Listener",     AddGroup::Audio,   insp_has<AudioListenerComponent>,    draw_audio_listener,     "AudioListener" },
				{ "Background Music",   AddGroup::Audio,   insp_has<BackgroundMusicComponent>,  draw_background_music,   "BackgroundMusic" },
			};
			return table;
		}

		void draw_add_component_menu(me::Entity entity, editor::CommandHistory& command_history) {
			ImGui::Dummy(ImVec2(0, 10));
			ImGui::Separator();
			ImGui::Dummy(ImVec2(0, 10));

			if (ImGui::Button("Add Component", ImVec2(ImGui::GetContentRegionAvail().x, 30))) {
				ImGui::OpenPopup("AddComponentPopup");
			}

			if (ImGui::BeginPopup("AddComponentPopup")) {
				bool first = true;
				AddGroup prev = AddGroup::Scene;
				for (const auto& c : inspector_components()) {
					if (!first && c.group != prev) ImGui::Separator();
					first = false;
					prev = c.group;
					if (!c.has(entity) && ImGui::MenuItem(c.label)) {
						command_history.AddCommand(std::make_unique<editor::AddComponentCommand>(
							me::get_registry(), entity.get_id(), c.meta_name));
					}
				}
				ImGui::EndPopup();
			}
		}

	} // namespace

	void InspectorPanel::on_imgui_render(me::Entity selected_entity, editor::CommandHistory& command_history) {
		ImGui::Begin("Inspector");

		if (selected_entity.is_valid()) {
			// Tag, Transform and Scripts are special (no remove button / bespoke UI),
			// so they're drawn explicitly; everything else comes from the table.
			draw_tag(selected_entity, command_history);
			draw_transform(selected_entity, command_history);

			for (const auto& c : inspector_components()) {
				if (c.draw) c.draw(selected_entity, command_history);
			}

			draw_script(selected_entity, command_history);
			draw_add_component_menu(selected_entity, command_history);
		} else {
			ImGui::Text("Select an entity to view its properties.");
		}

		ImGui::End();
	}

}
