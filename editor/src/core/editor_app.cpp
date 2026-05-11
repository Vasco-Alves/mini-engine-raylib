#include "editor/core/editor_app.hpp"

#include <string>

#include <imgui.h>
#include <rlImGui.h>

#include "editor/utils/file_dialogs.hpp"

#include <mini-engine-raylib/core/engine.hpp>
#include <mini-engine-raylib/core/vfs.hpp>
#include <mini-engine-raylib/input/input.hpp>
#include <mini-engine-raylib/render/renderer.hpp>
#include <mini-engine-raylib/ecs/components.hpp>
#include <mini-engine-raylib/scene/scene_manager.hpp>
#include <mini-engine-raylib/systems/camera_system.hpp>

#include <mini-ecs/registry.hpp>

namespace editor {

	// ====================================================================
	// CORE LIFECYCLE
	// ====================================================================

	void EditorApp::on_start() {
		me::input::bind_digital_axis("MoveY", me::input::Key::Q, me::input::Key::E, 1.0f);

		me::vfs::mount("engine", "assets");

		rlImGuiSetup(true);
		ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

		apply_theme();

		m_ViewportTexture = LoadRenderTexture(1920, 1080);
	}

	void EditorApp::on_resize(int width, int height) {}

	void EditorApp::on_update(float dt) {
		if (me::input::action_pressed("Quit") && !ImGui::GetIO().WantCaptureKeyboard) {
			me::close_application();
		}

		if (!m_IsProjectLoaded) return;

		// Camera Flying Logic
		if (m_SceneViewFocused && me::input::action_pressed("MouseRight")) {
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
	}

	void EditorApp::on_shutdown() {
		UnloadRenderTexture(m_ViewportTexture);
		rlImGuiShutdown();
	}

	// ====================================================================
	// MAIN RENDER LOOP (Now incredibly clean!)
	// ====================================================================

	void EditorApp::on_render() {
		if (!m_IsProjectLoaded) {
			draw_project_hub();
			return;
		}

		// 1. Dynamic Viewport Resizing
		if (m_ViewportTexture.texture.width != (int)m_ViewportBounds.x ||
			m_ViewportTexture.texture.height != (int)m_ViewportBounds.y) {
			if (m_ViewportBounds.x > 0 && m_ViewportBounds.y > 0) {
				UnloadRenderTexture(m_ViewportTexture);
				m_ViewportTexture = LoadRenderTexture((int)m_ViewportBounds.x, (int)m_ViewportBounds.y);
			}
		}

		// 2. Render Game to Texture
		BeginTextureMode(m_ViewportTexture);
		me::render::clear_world(me::Color{ 30, 30, 30, 255 });
		me::render::render_world(&m_EditorCameraTransform, &m_EditorCamera);
		EndTextureMode();

		// 3. Render Editor UI
		ClearBackground(BLACK);
		rlImGuiBegin();
		ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

		draw_menu_bar();
		draw_toolbar();

		// --- RENDER THE PANELS ---
		m_HierarchyPanel.on_imgui_render();
		m_BrowserPanel.on_imgui_render();

		// --- SCENE VIEWPORT ---
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
		ImGui::Begin("Scene View");
		m_SceneViewFocused = ImGui::IsWindowFocused() || ImGui::IsWindowHovered();
		ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
		m_ViewportBounds = { viewportPanelSize.x, viewportPanelSize.y };
		rlImGuiImageRenderTexture(&m_ViewportTexture);
		ImGui::End();
		ImGui::PopStyleVar();

		draw_modals();

		rlImGuiEnd();
	}

	// ====================================================================
	// UI HELPERS
	// ====================================================================

	void EditorApp::draw_menu_bar() {
		if (ImGui::BeginMainMenuBar()) {
			if (ImGui::BeginMenu("File")) {
				if (ImGui::MenuItem("New Scene")) {
					m_ShowNewSceneModal = true;
					strncpy(m_NewSceneInput, "my_new_scene", sizeof(m_NewSceneInput));
				}
				if (ImGui::MenuItem("Save Scene")) me::scene_manager::save(m_CurrentScenePath);
				ImGui::Separator();
				if (ImGui::MenuItem("Exit")) me::close_application();
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Layout")) {
				if (ImGui::MenuItem("Save Custom Layout")) ImGui::SaveIniSettingsToDisk("assets/custom_layout.ini");
				if (ImGui::MenuItem("Load Custom Layout")) ImGui::LoadIniSettingsFromDisk("assets/custom_layout.ini");
				ImGui::Separator();
				if (ImGui::MenuItem("Clear Current Layout")) {
					// ImGui::ClearIniSettings();
				}
				ImGui::EndMenu();
			}
			ImGui::EndMainMenuBar();
		}
	}

	void EditorApp::draw_toolbar() {
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 2));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 0));
		ImGui::SetNextWindowBgAlpha(0.0f);

		ImGui::Begin("##Toolbar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

		float button_size = 24.0f;
		ImGui::SetCursorPosX((ImGui::GetWindowContentRegionMax().x * 0.5f) - (button_size * 0.5f));

		if (m_SceneState == SceneState::Edit) {
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
			if (ImGui::Button("PLAY", ImVec2(60, button_size))) on_play();
			ImGui::PopStyleColor();
		} else {
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
			if (ImGui::Button("STOP", ImVec2(60, button_size))) on_stop();
			ImGui::PopStyleColor();
		}

		ImGui::End();
		ImGui::PopStyleVar(2);
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

				std::filesystem::path newPath = m_ProjectPath / "assets" / "scenes" / filename;

				new_scene();
				m_CurrentScenePath = newPath.lexically_normal().string();
				save_scene();

				m_ShowNewSceneModal = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120, 0))) {
				m_ShowNewSceneModal = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}

	// ====================================================================
	// SCENE & STATE MANAGEMENT
	// ====================================================================

	void EditorApp::new_scene() {
		me::scene_manager::clear();
		m_HierarchyPanel.set_selected_entity(0xFFFFFFFF); // Updated to use the Panel!

		auto& reg = me::get_registry();
		auto cube = reg.create_entity("Cube");
		cube.add_component(me::components::TagComponent{ "Cube" });
		cube.add_component(me::components::TransformComponent{ {0.0f, 0.05f, 0}, {0, 0, 0}, {1, 1, 1} });
		cube.add_component(me::components::Shape3DComponent{ me::components::Shape3DComponent::Cube, me::Color::white });
	}

	void EditorApp::save_scene() const {
		me::scene_manager::save(m_CurrentScenePath);
	}

	void EditorApp::on_play() {
		m_SceneState = SceneState::Play;
		me::scene_manager::save((m_ProjectPath / "assets" / "scenes" / ".temp_play.json").string());
		m_HierarchyPanel.set_selected_entity(0xFFFFFFFF); // Updated to use the Panel!
		me::set_playing(true);
	}

	void EditorApp::on_stop() {
		m_SceneState = SceneState::Edit;
		me::set_playing(false);
		me::scene_manager::load((m_ProjectPath / "assets" / "scenes" / ".temp_play.json").string());
		m_HierarchyPanel.set_selected_entity(0xFFFFFFFF); // Updated to use the Panel!
	}

	// ====================================================================
	// PROJECT HUB
	// ====================================================================

	void EditorApp::draw_project_hub() {
		ClearBackground(BLACK);
		rlImGuiBegin();

		// Center the window
		ImGui::SetNextWindowPos(ImVec2(GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(600, 220));

		ImGui::Begin("Project Hub", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

		ImGui::TextColored(ImVec4(0.74f, 0.58f, 0.98f, 1.0f), "Mini Engine | Project Hub");
		ImGui::Separator();
		ImGui::Dummy(ImVec2(0, 10));

		ImGui::Text("Project Path:");

		// 1. Make the text box slightly smaller to fit the Browse button next to it
		float inputWidth = ImGui::GetContentRegionAvail().x - 90.0f;
		ImGui::PushItemWidth(inputWidth);
		ImGui::InputText("##Path", m_ProjectInputPath, sizeof(m_ProjectInputPath));
		ImGui::PopItemWidth();

		ImGui::SameLine();

		// 2. The Native Browse Button
		if (ImGui::Button("Browse", ImVec2(80, 0))) {
			// This pauses the engine and opens the Windows/Mac folder picker!
			std::string dir = editor::utils::select_folder_dialog("Choose Project Folder", m_ProjectInputPath);

			// If the user didn't hit cancel, update the text box
			if (!dir.empty()) {
				// Normalize slashes for consistency
				std::replace(dir.begin(), dir.end(), '\\', '/');
				strncpy(m_ProjectInputPath, dir.c_str(), sizeof(m_ProjectInputPath) - 1);
			}
		}

		ImGui::Dummy(ImVec2(0, 20));

		// 3. The Unified "Smart Open" Button
		if (ImGui::Button("Open Project", ImVec2(ImGui::GetContentRegionAvail().x, 40))) {

			std::filesystem::path target_path = m_ProjectInputPath;

			// Check if the path exists. If not, create the base folder.
			if (!std::filesystem::exists(target_path)) {
				std::filesystem::create_directories(target_path);
			}

			// THE SMART CHECK: Does this folder already have an "assets" subfolder?
			if (std::filesystem::exists(target_path / "assets")) {
				// It's an existing project! Load it.
				load_project(target_path);
			} else {
				// It's empty! Generate the engine folders and bootstrap it.
				create_project(target_path);
			}
		}

		ImGui::End();
		rlImGuiEnd();
	}

	void EditorApp::create_project(const std::filesystem::path& path) {
		namespace fs = std::filesystem;
		fs::create_directories(path / "assets" / "scenes");
		fs::create_directories(path / "assets" / "scripts");
		fs::create_directories(path / "assets" / "models");
		fs::create_directories(path / "assets" / "textures");
		load_project(path);
	}

	void EditorApp::load_project(const std::filesystem::path& path) {
		namespace fs = std::filesystem;

		m_ProjectPath = path;
		m_IsProjectLoaded = true;

		me::vfs::mount("game", (m_ProjectPath / "assets").string());

		m_CurrentScenePath = me::vfs::resolve("game://scenes/main.json");

		// Wire up the Panels
		m_HierarchyPanel.set_context(&me::get_registry());
		m_BrowserPanel.set_project_path(path);

		m_BrowserPanel.on_scene_loaded = [this](const std::string& loaded_path) {
			m_CurrentScenePath = loaded_path;
			};

		m_BrowserPanel.on_entity_selection_cleared = [this]() {
			m_HierarchyPanel.set_selected_entity(0xFFFFFFFF);
			};

		if (fs::exists(m_CurrentScenePath)) {
			me::scene_manager::load(m_CurrentScenePath);
		} else {
			new_scene();
			save_scene();
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