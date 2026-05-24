#include "editor/core/editor_app.hpp"

#include <imgui.h>
#include <rlImGui.h>
#include <ImGuizmo.h>
#include <raymath.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>

#include <mini-engine-raylib/core/engine.hpp>
#include <mini-engine-raylib/core/vfs.hpp>
#include <mini-engine-raylib/core/file_system.hpp>
#include <mini-engine-raylib/core/events.hpp>
#include <mini-engine-raylib/core/logger.hpp>
#include <mini-engine-raylib/input/input.hpp>
#include <mini-engine-raylib/audio/audio.hpp>
#include <mini-engine-raylib/render/renderer.hpp>
#include <mini-engine-raylib/scene/scene_manager.hpp>
#include <mini-engine-raylib/ecs/script_component.hpp>
#include <mini-engine-raylib/ecs/audio_components.hpp>
#include <mini-engine-raylib/systems/camera_system.hpp>
#include <mini-engine-raylib/systems/physics_system.hpp>
#include <mini-engine-raylib/systems/audio_system.hpp>


namespace editor {

	void EditorApp::on_start() {
		me::input::bind_digital_axis("MoveY", me::input::Key::Q, me::input::Key::E, 1.0f);

		// Mount the root folder (where the executable lives)
		me::vfs::mount("root", ".");

		// Mount the engine assets folder
		me::vfs::mount("engine", "assets");

		rlImGuiSetup(true);
		ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		apply_theme();

		// Subscribe to Events
		auto& bus = me::get_event_bus();
		bus.subscribe<me::events::LogEvent>([](auto* e) { ConsolePanel::add_log(e->message, e->level); });
		bus.subscribe<me::events::SceneLoadedEvent>([this](auto* e) {
			if (e->filepath.find(".temp_play.json") == std::string::npos) {
				m_CurrentScenePath = e->filepath;
			}
			});
		bus.subscribe<me::events::EntitySelectedEvent>([this](auto* e) { m_HierarchyPanel.set_selected_entity(e->entity_id); });

		// Wire up the Project Hub Callbacks
		m_HubPanel.on_project_open = [this](const std::filesystem::path& p) { load_project(p); };
		m_HubPanel.on_project_create = [this](const std::filesystem::path& p) { create_project(p); };

		load_engine_config();
		m_ViewportPanel.on_start();

		me::render::init();
		me::physics::init();
	}

	void EditorApp::on_shutdown() {
		me::physics::shutdown();
		me::render::shutdown();
		m_ViewportPanel.on_shutdown();
		rlImGuiShutdown();
	}

	void EditorApp::on_resize(int width, int height) {}

	void EditorApp::on_update(float dt) {
		if (me::input::action_pressed("Quit") && !ImGui::GetIO().WantCaptureKeyboard) {
			me::close_application();
		}

		if (!m_IsProjectLoaded) return;

		// OS-Level File Dropping
		if (IsFileDropped()) {
			FilePathList dropped_files = LoadDroppedFiles();
			std::filesystem::path target_dir = m_BrowserPanel.get_current_directory();

			for (int i = 0; i < dropped_files.count; i++) {
				try {
					std::filesystem::copy(dropped_files.paths[i], target_dir / std::filesystem::path(dropped_files.paths[i]).filename(),
						std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing);
					me::logger::info("Imported: " + std::filesystem::path(dropped_files.paths[i]).filename().string());
				} catch (const std::exception& e) {
					me::logger::error("Failed to import file: " + std::string(e.what()));
				}
			}
			UnloadDroppedFiles(dropped_files);
		}

		// Editor Camera Flying
		if (m_ViewportPanel.is_hovered() && me::input::action_pressed("MouseRight")) {
			m_IsFlying = true;
			me::input::lock_cursor();
		}

		if (m_IsFlying) {
			me::camera::update_editor_camera(m_EditorCameraTransform, m_EditorCamera, dt);
			if (me::input::action_released("MouseRight")) {
				m_IsFlying = false;
				me::input::unlock_cursor();
			}
		}

		// Step the physics simulation
		if (me::is_playing()) {
			if (!me::is_paused() || m_StepPhysicsNextFrame) {

				float physics_dt = me::is_paused() ? (1.0f / 60.0f) : dt;
				me::physics::update(me::get_registry(), physics_dt);

				m_StepPhysicsNextFrame = false;
			}
		}

		poll_shortcuts();

		// ==========================================
		// 3D AUDIO SPATIAL UPDATE (Moved outside the physics if-statement!)
		// ==========================================
		// Calculate the Editor Camera's forward vector
		Vector3 editor_pos = { m_EditorCameraTransform.position.x, m_EditorCameraTransform.position.y, m_EditorCameraTransform.position.z };
		Vector3 editor_target = { m_EditorCamera.target.x, m_EditorCamera.target.y, m_EditorCamera.target.z };
		Vector3 editor_forward = Vector3Normalize(Vector3Subtract(editor_target, editor_pos));
		Vector3 editor_up = { m_EditorCamera.up.x, m_EditorCamera.up.y, m_EditorCamera.up.z };

		me::systems::audio_update(me::get_registry(), editor_pos, editor_forward, editor_up);
	}

	void EditorApp::on_render() {
		if (!m_IsProjectLoaded) {
			m_HubPanel.on_imgui_render();
			return;
		}

		// 1. Render the 3D World to the Viewport Texture
		m_ViewportPanel.begin_render();

		me::render::clear_world(me::Color{ 30, 30, 30, 255 });
		me::render::render_world(&m_EditorCameraTransform, &m_EditorCamera);

		// --- Pack ECS data into a raw Raylib struct to draw the grid & gizmos ---
		Camera3D gridCam = { 0 };
		gridCam.position = { m_EditorCameraTransform.position.x, m_EditorCameraTransform.position.y, m_EditorCameraTransform.position.z };
		gridCam.target = { m_EditorCamera.target.x, m_EditorCamera.target.y, m_EditorCamera.target.z };
		gridCam.up = { m_EditorCamera.up.x, m_EditorCamera.up.y, m_EditorCamera.up.z };
		gridCam.fovy = m_EditorCamera.fov;
		gridCam.projection = CAMERA_PERSPECTIVE;

		BeginMode3D(gridCam);

		// Draw the baseline editor grid
		DrawGrid(100, 1.0f);

		// ==========================================
		// DRAW EDITOR GIZMOS (Lights, Cameras, etc.)
		// ==========================================
		if (m_SceneState == SceneState::Edit) {
			auto& reg = me::get_registry();

			// 1. Point Light Gizmos (A simple wireframe sphere)
			auto& light_pool = reg.view<me::components::LightComponent>();
			for (size_t i = 0; i < light_pool.size(); ++i) {
				me::entity::entity_id e = light_pool.entity_map[i];
				if (auto* t = reg.try_get_component<me::components::TransformComponent>(e)) {
					auto& l = light_pool.components[i];
					::Color c = { l.color.r, l.color.g, l.color.b, 255 };
					DrawSphereWires({ t->position.x, t->position.y, t->position.z }, 0.25f, 8, 8, c);
				}
			}

			// 2. Directional Light Gizmo (A sphere with a directional arrow)
			auto& dir_pool = reg.view<me::components::DirectionalLightComponent>();
			for (size_t i = 0; i < dir_pool.size(); ++i) {
				me::entity::entity_id e = dir_pool.entity_map[i];
				if (auto* t = reg.try_get_component<me::components::TransformComponent>(e)) {
					auto& dl = dir_pool.components[i];
					::Color c = { dl.color.r, dl.color.g, dl.color.b, 255 };
					Vector3 pos = { t->position.x, t->position.y, t->position.z };

					// Calculate the exact forward direction based on rotation
					float pitch = t->rotation.x * DEG2RAD;
					float yaw = t->rotation.y * DEG2RAD;
					Vector3 dir = {
						std::cos(pitch) * std::sin(yaw),
						-std::sin(pitch),
						std::cos(pitch) * std::cos(yaw)
					};
					dir = Vector3Normalize(dir);

					// Draw the Sun origin and the line pointing out
					DrawSphereWires(pos, 0.5f, 12, 12, c);

					Vector3 endPos = { pos.x + dir.x * 3.0f, pos.y + dir.y * 3.0f, pos.z + dir.z * 3.0f };
					DrawLine3D(pos, endPos, c);

					// Draw a tiny solid sphere at the end to act as an arrowhead
					DrawSphere(endPos, 0.15f, c);
				}
			}
		}

		// --- Draw Physics Hitboxes ---
		me::physics::draw_debug(me::get_registry());

		EndMode3D();
		m_ViewportPanel.end_render();

		// 2. Render the UI
		ClearBackground(BLACK);
		rlImGuiBegin();
		ImGuizmo::BeginFrame();

		// ==========================================
		// DEFERRED UI LAYOUT LOADING & SAVING
		// ==========================================
		if (m_WantsToSaveLayout) {
			std::string path = me::vfs::resolve("root://custom_layout.ini");
			ImGui::SaveIniSettingsToDisk(path.c_str());
			me::logger::info("Saved custom layout to: " + path);
			m_WantsToSaveLayout = false;
		}

		if (m_WantsToLoadLayout) {
			std::string path = me::vfs::resolve("root://custom_layout.ini");
			ImGui::LoadIniSettingsFromDisk(path.c_str());
			me::logger::info("Loaded custom layout from: " + path);
			m_WantsToLoadLayout = false;
		}

		ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

		draw_menu_bar();
		draw_toolbar();

		// Draw Panels
		m_HierarchyPanel.on_imgui_render();
		m_InspectorPanel.on_imgui_render(&me::get_registry(), m_HierarchyPanel.get_selected_entity());
		m_BrowserPanel.on_imgui_render();
		m_ConsolePanel.on_imgui_render();

		// Pass the exact camera and selection state down into the Viewport
		m_ViewportPanel.on_imgui_render(m_EditorCameraTransform, m_EditorCamera, m_HierarchyPanel.get_selected_entity(), m_GizmoType);

		draw_modals();

		rlImGuiEnd();
	}

	void EditorApp::poll_shortcuts() {
		bool ctrl = ImGui::GetIO().KeyCtrl;
		bool wantText = ImGui::GetIO().WantTextInput;
		me::entity::entity_id selected = m_HierarchyPanel.get_selected_entity();

		// Global shortcuts — always fire (unless typing in a text box)
		if (!wantText) {

			// 1. Save Scene (Ctrl + S)
			if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
				save_scene();
				me::logger::info("Saved scene in " + m_CurrentScenePath);
			}

			// 2. New Scene (Ctrl + N)
			if (ctrl && ImGui::IsKeyPressed(ImGuiKey_N)) {
				m_ShowNewSceneModal = true;
				strncpy(m_NewSceneInput, "my_new_scene", sizeof(m_NewSceneInput));
			}

			// 3. Play / Stop toggle (Ctrl + P)
			if (ctrl && ImGui::IsKeyPressed(ImGuiKey_P)) {
				if (m_SceneState == SceneState::Edit) { on_play(); me::set_paused(false); } else                                    on_stop();
			}
		}

		// Entity shortcuts — only when not flying and not typing
		if (!m_IsFlying && !wantText) {

			// 4. Escape to deselect
			if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
				m_HierarchyPanel.set_selected_entity(0xFFFFFFFF);
			}

			// 5. Delete selected entity
			if (selected != 0xFFFFFFFF && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
				auto& reg = me::get_registry();
				if (auto* t = reg.try_get_component<me::components::TransformComponent>(selected)) {
					// Unlink from parent
					if (t->parent != me::entity::null) {
						if (auto* p = reg.try_get_component<me::components::TransformComponent>(t->parent))
							p->remove_child(selected);
					}
					// Orphan all children so they become root entities
					for (auto child_id : t->children) {
						if (auto* ct = reg.try_get_component<me::components::TransformComponent>(child_id))
							ct->parent = me::entity::null;
					}
				}
				reg.destroy_entity(selected);
				m_HierarchyPanel.set_selected_entity(0xFFFFFFFF);
			}

			// 6. Duplicate entity (Ctrl + D)
			if (selected != 0xFFFFFFFF && ctrl && ImGui::IsKeyPressed(ImGuiKey_D)) {
				auto& reg = me::get_registry();

				auto old_tag = reg.try_get_component<me::components::TagComponent>(selected);
				std::string new_name = old_tag ? (old_tag->name + " (Clone)") : "Entity (Clone)";

				auto new_ent = reg.create_entity();
				new_ent.add_component<me::components::TagComponent>({ new_name });

				if (auto* t = reg.try_get_component<me::components::TransformComponent>(selected))
					new_ent.add_component<me::components::TransformComponent>(*t);
				if (auto* s = reg.try_get_component<me::components::Shape3DComponent>(selected))
					new_ent.add_component<me::components::Shape3DComponent>(*s);
				if (auto* m = reg.try_get_component<me::components::Model3DComponent>(selected))
					new_ent.add_component<me::components::Model3DComponent>(*m);
				if (auto* l = reg.try_get_component<me::components::LightComponent>(selected))
					new_ent.add_component<me::components::LightComponent>(*l);
				if (auto* dl = reg.try_get_component<me::components::DirectionalLightComponent>(selected))
					new_ent.add_component<me::components::DirectionalLightComponent>(*dl);
				if (auto* c = reg.try_get_component<me::components::CameraComponent>(selected))
					new_ent.add_component<me::components::CameraComponent>(*c);
				if (auto* s2 = reg.try_get_component<me::components::Shape2DComponent>(selected))
					new_ent.add_component<me::components::Shape2DComponent>(*s2);
				if (auto* sp = reg.try_get_component<me::components::SpriteComponent>(selected))
					new_ent.add_component<me::components::SpriteComponent>(*sp);
				if (auto* c2 = reg.try_get_component<me::components::Camera2DComponent>(selected))
					new_ent.add_component<me::components::Camera2DComponent>(*c2);
				if (auto* sc = reg.try_get_component<me::components::ScriptComponent>(selected)) {
					me::components::ScriptComponent new_sc;
					for (const auto& script : sc->scripts) new_sc.scripts.push_back({ script.path });
					new_ent.add_component<me::components::ScriptComponent>(new_sc);
				}

				m_HierarchyPanel.set_selected_entity(new_ent.get_id());
			}
		}

		// Viewport-focused shortcuts — only when the viewport has focus and we're not flying
		if (m_ViewportPanel.is_focused() && !m_IsFlying && !wantText) {

			// 7. Frame selected entity (F)
			if (selected != 0xFFFFFFFF && ImGui::IsKeyPressed(ImGuiKey_F)) {
				auto* transform = me::get_registry().try_get_component<me::components::TransformComponent>(selected);
				if (transform) {
					m_EditorCamera.target = { transform->position.x, transform->position.y, transform->position.z };
					float distance = 10.0f;
					m_EditorCameraTransform.position.x = transform->position.x - std::sin(m_EditorCameraTransform.rotation.y * (PI / 180.0f)) * distance;
					m_EditorCameraTransform.position.y = transform->position.y + 5.0f;
					m_EditorCameraTransform.position.z = transform->position.z - std::cos(m_EditorCameraTransform.rotation.y * (PI / 180.0f)) * distance;
					m_EditorCameraTransform.rotation.x = -25.0f;
				}
			}

			// 8. Gizmo tool switching (Q / W / R / S)
			if (ImGui::IsKeyPressed(ImGuiKey_Q)) m_GizmoType = -1;
			if (ImGui::IsKeyPressed(ImGuiKey_W)) m_GizmoType = ImGuizmo::TRANSLATE;
			if (ImGui::IsKeyPressed(ImGuiKey_R)) m_GizmoType = ImGuizmo::ROTATE;
			if (ImGui::IsKeyPressed(ImGuiKey_S)) m_GizmoType = ImGuizmo::SCALE;
		}
	}

	// ====================================================================
	// PROJECT & SCENE LOGIC
	// ====================================================================

	void EditorApp::create_project(const std::filesystem::path& path) {
		me::vfs::mount("game", (path / "assets").string());
		me::fs::create_directory("game://scenes");
		me::fs::create_directory("game://scripts");
		me::fs::create_directory("game://models");
		me::fs::create_directory("game://textures");
		me::fs::create_directory("game://audio");
		load_project(path);
	}

	void EditorApp::load_project(const std::filesystem::path& path) {
		add_recent_project(path.generic_string());

		m_ProjectPath = path;
		m_IsProjectLoaded = true;
		me::vfs::mount("game", (m_ProjectPath / "assets").string());
		m_CurrentScenePath = me::vfs::resolve("game://scenes/main.json");

		m_HierarchyPanel.set_context(&me::get_registry());
		m_BrowserPanel.set_project_path(path);

		if (me::fs::exists("game://scenes/main.json")) me::scene_manager::load("game://scenes/main.json");
		else { new_scene(); save_scene(); }
	}

	void EditorApp::new_scene() {
		me::scene_manager::clear();
		m_HierarchyPanel.set_selected_entity(0xFFFFFFFF);

		// Create a Directional Light Source (The Sun)
		auto sun = me::get_registry().create_entity();
		sun.add_component(me::components::TagComponent{ "Directional Light" });
		sun.add_component(me::components::TransformComponent{
			{0.0f, 20.0f, 0.0f},   // Position
			{-45.0f, 45.0f, 0.0f}, // Rotation
			{1.0f, 1.0f, 1.0f}     // Scale
			});
		sun.add_component(me::components::DirectionalLightComponent{ me::Color::white, 1.0f });

		// Create a simple Cube
		auto cube = me::get_registry().create_entity();
		cube.add_component(me::components::TagComponent{ "Cube" });
		cube.add_component(me::components::TransformComponent{ {0.0f, 0.05f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f} });
		cube.add_component(me::components::Shape3DComponent{ me::components::Shape3DComponent::Cube, me::Color::white });
	}

	void EditorApp::save_scene() const { me::scene_manager::save(m_CurrentScenePath); }

	void EditorApp::on_play() {
		m_SceneState = SceneState::Play;
		me::logger::info("Mode set to: PLAYING");
		me::scene_manager::save("game://scenes/.temp_play.json");

		// Start the physics simulation and load the bodies
		me::physics::on_play(me::get_registry());

		me::set_playing(true);
		me::get_event_bus().publish<me::events::PlayStateChangedEvent>(true);
	}

	void EditorApp::on_stop() {
		m_SceneState = SceneState::Edit;
		me::logger::info("Mode set to: EDIT");
		me::set_playing(false);
		me::get_event_bus().publish<me::events::PlayStateChangedEvent>(false);

		// Destroy all live physics bodies
		me::physics::on_stop();

		// ==========================================
		// SILENCE ALL ACTIVE AUDIO
		// ==========================================
		auto& registry = me::get_registry();

		// Stop any looping music streams
		auto& music_pool = registry.view<me::components::BackgroundMusicComponent>();
		for (size_t i = 0; i < music_pool.size(); ++i) {
			me::audio::stop_music(music_pool.components[i].stream);
		}

		// Stop any long-playing sound effects
		auto& sfx_pool = registry.view<me::components::AudioSourceComponent>();
		for (size_t i = 0; i < sfx_pool.size(); ++i) {
			me::audio::stop(sfx_pool.components[i].clip);
		}

		me::scene_manager::load("game://scenes/.temp_play.json");
		me::fs::remove("game://scenes/.temp_play.json");
	}

	void EditorApp::load_engine_config() {
		std::ifstream ifs("engine_config.json");
		if (ifs.is_open()) {
			nlohmann::json j;
			try {
				ifs >> j;
				if (j.contains("recent_projects")) {
					for (const auto& path : j["recent_projects"]) m_RecentProjects.push_back(path.get<std::string>());
					m_HubPanel.set_recent_projects(m_RecentProjects); // Pass to hub!
				}
				if (j.contains("ui_scale")) ImGui::GetIO().FontGlobalScale = j["ui_scale"].get<float>();
			} catch (...) {}
		}
	}

	void EditorApp::save_engine_config() {
		nlohmann::json j;
		j["recent_projects"] = m_RecentProjects;
		j["ui_scale"] = ImGui::GetIO().FontGlobalScale;
		std::ofstream ofs("engine_config.json");
		ofs << j.dump(4);
	}

	void EditorApp::add_recent_project(const std::string& path) {
		auto it = std::remove(m_RecentProjects.begin(), m_RecentProjects.end(), path);
		m_RecentProjects.erase(it, m_RecentProjects.end());
		m_RecentProjects.insert(m_RecentProjects.begin(), path);
		if (m_RecentProjects.size() > 10) m_RecentProjects.pop_back();
		m_HubPanel.set_recent_projects(m_RecentProjects); // Update hub!
		save_engine_config();
	}

	// ====================================================================
	// UI DRAW HELPERS
	// ====================================================================

	void EditorApp::draw_menu_bar() {
		if (ImGui::BeginMainMenuBar()) {

			// --- File Menu ---
			if (ImGui::BeginMenu("File")) {
				if (ImGui::MenuItem("New Scene")) {
					m_ShowNewSceneModal = true;
					strncpy(m_NewSceneInput, "my_new_scene", sizeof(m_NewSceneInput));
				}
				if (ImGui::MenuItem("Save Scene")) save_scene();
				ImGui::Separator();
				if (ImGui::MenuItem("Exit")) me::close_application();
				ImGui::EndMenu();
			}

			// --- Layout Menu ---
			if (ImGui::BeginMenu("Layout")) {

				if (ImGui::MenuItem("Save Custom Layout")) {
					m_WantsToSaveLayout = true; // Tell the engine to save it later
				}

				if (ImGui::MenuItem("Load Custom Layout")) {
					m_WantsToLoadLayout = true; // Tell the engine to load it later
				}

				ImGui::EndMenu();
			}

			// --- View Menu ---
			if (ImGui::BeginMenu("View")) {

				// Lighting Toggle Checkbox
				bool lighting = me::render::is_lighting_enabled();
				if (ImGui::MenuItem("Lit Mode (Lighting)", nullptr, &lighting)) {
					me::render::set_lighting_enabled(lighting);
				}

				ImGui::Separator();
				ImGui::TextDisabled("UI Scale");
				ImGui::Separator();

				float current_scale = ImGui::GetIO().FontGlobalScale;
				float scales[] = { 0.75f, 1.0f, 1.25f, 1.5f, 2.0f };
				const char* scale_labels[] = { "75%", "100% (Default)", "125%", "150%", "200%" };

				for (int i = 0; i < 5; ++i) {
					if (ImGui::MenuItem(scale_labels[i], "", current_scale == scales[i])) {
						ImGui::GetIO().FontGlobalScale = scales[i];
						save_engine_config();
					}
				}

				ImGui::EndMenu();
			}

			ImGui::EndMainMenuBar();
		}
	}

	void EditorApp::draw_toolbar() {
		ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 0));
		ImGui::SetNextWindowBgAlpha(0.0f);
		ImGui::Begin("##Toolbar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

		float button_size = 26.0f;

		ImGui::PushStyleColor(ImGuiCol_Button, m_GizmoType == ImGuizmo::TRANSLATE ? ImVec4(0.2f, 0.6f, 0.9f, 1.0f) : ImVec4(0.16f, 0.16f, 0.21f, 1.0f));
		if (ImGui::Button("T", ImVec2(button_size, button_size))) m_GizmoType = ImGuizmo::TRANSLATE;
		ImGui::PopStyleColor(); ImGui::SameLine(0, 5);

		ImGui::PushStyleColor(ImGuiCol_Button, m_GizmoType == ImGuizmo::ROTATE ? ImVec4(0.2f, 0.6f, 0.9f, 1.0f) : ImVec4(0.16f, 0.16f, 0.21f, 1.0f));
		if (ImGui::Button("R", ImVec2(button_size, button_size))) m_GizmoType = ImGuizmo::ROTATE;
		ImGui::PopStyleColor(); ImGui::SameLine(0, 5);

		ImGui::PushStyleColor(ImGuiCol_Button, m_GizmoType == ImGuizmo::SCALE ? ImVec4(0.2f, 0.6f, 0.9f, 1.0f) : ImVec4(0.16f, 0.16f, 0.21f, 1.0f));
		if (ImGui::Button("S", ImVec2(button_size, button_size))) m_GizmoType = ImGuizmo::SCALE;
		ImGui::PopStyleColor();

		if (m_SceneState == SceneState::Edit) {
			ImGui::SameLine((ImGui::GetWindowContentRegionMax().x * 0.5f) - (60 * 0.5f));
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
			if (ImGui::Button("PLAY", ImVec2(60, button_size))) { on_play(); me::set_paused(false); }
			ImGui::PopStyleColor();
		} else {
			float total_width = 60 + 5 + 60 + 5 + 60;
			ImGui::SameLine((ImGui::GetWindowContentRegionMax().x * 0.5f) - (total_width * 0.5f));

			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
			if (ImGui::Button("STOP", ImVec2(60, button_size))) on_stop();
			ImGui::PopStyleColor(); ImGui::SameLine(0, 5);

			bool is_paused = me::is_paused();
			ImGui::PushStyleColor(ImGuiCol_Button, is_paused ? ImVec4(0.2f, 0.6f, 0.9f, 1.0f) : ImVec4(0.8f, 0.6f, 0.1f, 1.0f));
			if (ImGui::Button(is_paused ? "RESUME" : "PAUSE", ImVec2(60, button_size))) {
				me::set_paused(!is_paused);
				me::get_event_bus().publish<me::events::PauseStateChangedEvent>(!is_paused);
			}
			ImGui::PopStyleColor(); ImGui::SameLine(0, 5);

			if (!is_paused) ImGui::BeginDisabled();
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
			if (ImGui::Button("STEP", ImVec2(60, button_size))) {
				me::step(1);
				m_StepPhysicsNextFrame = true;
			}
			ImGui::PopStyleColor();
			if (!is_paused) ImGui::EndDisabled();
		}
		ImGui::End();
		ImGui::PopStyleVar(1);
	}

	void EditorApp::draw_modals() {
		if (m_ShowNewSceneModal) ImGui::OpenPopup("Create New Scene");
		ImGui::SetNextWindowPos(ImVec2(GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal("Create New Scene", &m_ShowNewSceneModal, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("Enter scene name:");
			ImGui::InputText("##SceneName", m_NewSceneInput, sizeof(m_NewSceneInput));
			ImGui::Dummy(ImVec2(0, 10));

			if (ImGui::Button("Create", ImVec2(120, 0))) {
				std::string filename = std::string(m_NewSceneInput);
				if (filename.find(".json") == std::string::npos) filename += ".json";
				m_CurrentScenePath = "game://scenes/" + filename;
				new_scene();
				save_scene();
				m_ShowNewSceneModal = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120, 0))) { m_ShowNewSceneModal = false; ImGui::CloseCurrentPopup(); }
			ImGui::EndPopup();
		}
	}

	// ====================================================================
	// THEME
	// ====================================================================

	void EditorApp::apply_theme() {
		ImGuiStyle& style = ImGui::GetStyle();
		auto& colors = style.Colors;

		colors[ImGuiCol_WindowBg] = ImVec4{ 0.11f, 0.11f, 0.13f, 1.0f };
		colors[ImGuiCol_MenuBarBg] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };
		colors[ImGuiCol_Border] = ImVec4{ 0.44f, 0.37f, 0.61f, 0.29f };
		colors[ImGuiCol_BorderShadow] = ImVec4{ 0.0f, 0.0f, 0.0f, 0.24f };
		colors[ImGuiCol_Text] = ImVec4{ 1.0f, 1.0f, 1.0f, 1.0f };
		colors[ImGuiCol_TextDisabled] = ImVec4{ 0.5f, 0.5f, 0.5f, 1.0f };
		colors[ImGuiCol_Header] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };
		colors[ImGuiCol_HeaderHovered] = ImVec4{ 0.24f, 0.24f, 0.32f, 1.0f };
		colors[ImGuiCol_HeaderActive] = ImVec4{ 0.20f, 0.20f, 0.27f, 1.0f };
		colors[ImGuiCol_Button] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };
		colors[ImGuiCol_ButtonHovered] = ImVec4{ 0.24f, 0.24f, 0.32f, 1.0f };
		colors[ImGuiCol_ButtonActive] = ImVec4{ 0.20f, 0.20f, 0.27f, 1.0f };
		colors[ImGuiCol_CheckMark] = ImVec4{ 0.74f, 0.58f, 0.98f, 1.0f };
		colors[ImGuiCol_SliderGrab] = ImVec4{ 0.44f, 0.37f, 0.61f, 0.54f };
		colors[ImGuiCol_SliderGrabActive] = ImVec4{ 0.74f, 0.58f, 0.98f, 0.54f };
		colors[ImGuiCol_Tab] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };
		colors[ImGuiCol_TabHovered] = ImVec4{ 0.24f, 0.24f, 0.32f, 1.0f };
		colors[ImGuiCol_TabActive] = ImVec4{ 0.20f, 0.20f, 0.27f, 1.0f };
		colors[ImGuiCol_TitleBg] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };
		colors[ImGuiCol_TitleBgActive] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };

		style.WindowPadding = ImVec2(8.0f, 8.0f);
		style.FramePadding = ImVec2(5.0f, 3.0f);
		style.CellPadding = ImVec2(6.0f, 6.0f);
		style.ItemSpacing = ImVec2(6.0f, 6.0f);
		style.ItemInnerSpacing = ImVec2(6.0f, 6.0f);
		style.TouchExtraPadding = ImVec2(0.0f, 0.0f);
		style.IndentSpacing = 25;
		style.ScrollbarSize = 15;
		style.GrabMinSize = 10;
		style.WindowBorderSize = 1;
		style.ChildBorderSize = 1;
		style.PopupBorderSize = 1;
		style.FrameBorderSize = 1;
		style.TabBorderSize = 1;
		style.WindowRounding = 4;
		style.ChildRounding = 4;
		style.FrameRounding = 3;
		style.PopupRounding = 4;
		style.ScrollbarRounding = 9;
		style.GrabRounding = 3;
		style.TabRounding = 4;
	}

} // namespace editor
