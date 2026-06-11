#pragma once

#include <raylib.h>
#include <string>
#include <vector>
#include <filesystem>

#include <mini-engine-raylib/core/application.hpp>
#include <mini-engine-raylib/ecs/components.hpp>
#include <mini-engine-raylib/systems/raytracer_system.hpp>

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
		Play = 1,
		Render = 2
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
		void draw_overlays();
		void apply_theme();
		void update_window_title();

		// Caps the frame rate to the monitor refresh in Edit/Play (no point
		// rendering the UI faster), uncaps it in Render (every frame = a sample).
		void apply_frame_pacing();

		// --- Entity Operations (undoable, shared by shortcuts and the Edit menu) ---
		void delete_selected_entity();
		void duplicate_selected_entity();

		// --- Project Management ---
		void create_project(const std::filesystem::path& path);
		void load_project(const std::filesystem::path& path);
		void load_engine_config();
		void save_engine_config();
		void add_recent_project(const std::string& path);

		// --- Scene Management ---
		void new_scene();
		void save_scene();
		void open_save_as_modal();
		void open_scene(const std::string& vfs_path);
		void on_play();
		void on_stop();

		// --- Unsaved-changes guard ---
		// request_* check m_SceneDirty first; if dirty they park the action and
		// show the "Unsaved Changes" modal, which then calls perform_pending_action.
		enum class PendingAction { None, NewScene, OpenScene, Exit };
		void request_exit();
		void request_new_scene();
		void request_open_scene(const std::string& vfs_path);
		void perform_pending_action();

	private:
		// --- State Variables ---
		bool m_IsProjectLoaded = false;
		std::filesystem::path m_ProjectPath;
		std::string m_CurrentScenePath;
		SceneState m_SceneState = SceneState::Edit;
		std::vector<std::string> m_RecentProjects;

		char m_NewSceneInput[256] = "my_new_scene";
		int m_GizmoType = 7; // ImGuizmo::TRANSLATE

		// --- Modals ---
		bool m_ShowNewSceneModal = false;
		bool m_ShowSaveAsModal = false;
		bool m_ShowUnsavedModal = false;
		PendingAction m_PendingAction = PendingAction::None;
		std::string m_PendingScenePath;

		// --- Unsaved-changes marker (window title "*") ---
		bool m_SceneDirty = false;
		std::string m_LastWindowTitle;

		// --- Overlays ---
		bool m_ShowStats = false;        // View menu toggle: FPS / entities / samples
		float m_FlySpeedToastTimer = 0.0f; // shows fly speed briefly after scrolling

		// --- Export State Tracking ---
		bool m_ShowExportModal = false;
		bool m_IsExporting = false;
		int m_PreviewW = 0;
		int m_PreviewH = 0;
		// Viewport settings the export temporarily overrides, restored after.
		int m_ExportSavedPreviewSamples = 50;
		bool m_ExportSavedAccumulate = false;
		int m_ExportSavedBounces = 3;
		std::string m_ExportPath = "";

		// --- Camera ---
		me::components::CameraComponent m_EditorCamera;
		me::components::TransformComponent m_EditorCameraTransform = {
			{0.0f, 5.0f, 10.0f},
			{-25.0f, 180.0f, 0.0f},
			{1.0f, 1.0f, 1.0f}
		};
		bool m_IsFlying = false;
		Vector3 m_OrbitTarget = { 0.0f, 0.0f, 0.0f };

		// --- Sub-Systems ---
		me::systems::RaytracerSystem m_Raytracer;

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