#pragma once

#include <raylib.h>
#include <string>
#include <vector>
#include <filesystem>

#include <mini-engine-raylib/core/application.hpp>
#include <mini-engine-raylib/ecs/components.hpp>

// --- Panels ---
#include "editor/panels/scene_hierarchy_panel.hpp"
#include "editor/panels/inspector_panel.hpp"
#include "editor/panels/content_browser_panel.hpp"
#include "editor/panels/console_panel.hpp"
#include "editor/panels/viewport_panel.hpp"
#include "editor/panels/project_hub_panel.hpp"

namespace editor {

	enum class SceneState {
		Edit = 0,
		Play = 1
	};

	class EditorApp : public me::Application {
	public:
		EditorApp() = default;
		~EditorApp() = default;

		void on_start() override;
		void on_update(float dt) override;
		void on_render() override;
		void on_shutdown() override;
		void on_resize(int width, int height) override;

	private:
		// --- Events ---
		void subcribe_events();

		// --- Sub-Systems ---
		void poll_shortcuts();
		void draw_menu_bar();
		void draw_toolbar();
		void draw_modals();
		void apply_theme();

		// --- Project Management ---
		void create_project(const std::filesystem::path& path);
		void load_project(const std::filesystem::path& path);
		void load_engine_config();
		void save_engine_config();
		void add_recent_project(const std::string& path);

		// --- Scene Management ---
		void new_scene();
		void save_scene() const;
		void on_play();
		void on_stop();

	private:
		// --- State Variables ---
		bool m_IsProjectLoaded = false;
		std::filesystem::path m_ProjectPath;
		std::string m_CurrentScenePath;
		SceneState m_SceneState = SceneState::Edit;
		std::vector<std::string> m_RecentProjects;

		bool m_ShowNewSceneModal = false;
		char m_NewSceneInput[256] = "my_new_scene";
		int m_GizmoType = 7; // ImGuizmo::TRANSLATE

		// --- Camera ---
		me::components::CameraComponent m_EditorCamera;
		me::components::TransformComponent m_EditorCameraTransform = {
			{0.0f, 5.0f, 10.0f},
			{-25.0f, 180.0f, 0.0f},
			{1.0f, 1.0f, 1.0f}
		};
		bool m_IsFlying = false;
		Vector3 m_OrbitTarget = { 0.0f, 0.0f, 0.0f };

		// --- Physics ---
		bool m_StepPhysicsNextFrame = false;

		// --- UI Panels ---
		SceneHierarchyPanel m_HierarchyPanel;
		InspectorPanel m_InspectorPanel;
		ContentBrowserPanel m_BrowserPanel;
		ConsolePanel m_ConsolePanel;
		ViewportPanel m_ViewportPanel;
		ProjectHubPanel m_HubPanel;

		// --- Layout ---
		bool m_WantsToSaveLayout = false;
		bool m_WantsToLoadLayout = false;

		// -- Command History --
		editor::CommandHistory m_CommandHistory;
	};

} // namespace editor