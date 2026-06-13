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
#include "editor/panels/animation_panel.hpp"

#include "editor/core/scene_animation.hpp"

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
		// PNG-export + animation-render dialogs and their progress/render loops
		// (defined in editor_offline_render.cpp).
		void draw_offline_render_modals();
		void draw_overlays();
		void apply_theme();
		void update_window_title();

		// Caps the frame rate to the monitor refresh in Edit/Play (no point
		// rendering the UI faster), uncaps it in Render (every frame = a sample).
		void apply_frame_pacing();

		// Swaps dock layouts when crossing Edit <-> Render (Play shares Edit's).
		void switch_mode_layout(SceneState from, SceneState to);

		// --- Layout persistence (explicit-save model) ---
		// ImGui's continuous imgui.ini autosave is disabled. Within a session,
		// mode switches keep your arrangement in memory; across sessions only
		// what you saved with "Save Layout" comes back. Fresh installs fall back
		// to the defaults shipped in assets/layouts/.
		void save_current_layout();      // writes layout_<mode>.ini next to the exe
		void reset_layout_to_default();  // reloads the shipped default for this mode
		void save_layout_as_default();   // exports current layout into assets/layouts/

		// --- Entity Operations (undoable, shared by shortcuts and the Edit menu) ---
		void delete_selected_entity();
		void duplicate_selected_entity();

		// --- Project Management ---
		void create_project(const std::filesystem::path& path);
		void load_project(const std::filesystem::path& path);
		void load_engine_config();
		void save_engine_config();
		void add_recent_project(const std::string& path);

		// Packages the prebuilt game runtime (game.exe, built with the engine)
		// together with this project's assets into a standalone game folder.
		void export_game();

		// --- Animation (camera track + entity component tracks) ---
		// The tracks live in a sidecar next to the scene ("foo.anim.json"),
		// saved with Ctrl+S and loaded whenever the scene loads.
		std::string anim_sidecar_path() const;
		void save_animation_sidecar();
		void load_animation_sidecar();
		void start_animation_render();
		// Moves camera + entity tracks to time t and restarts accumulation
		// (with a transform + BVH/SSBO rebuild when entity tracks exist).
		void apply_animation_at(float t);

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

		// --- Game export (File > Export Game...) ---
		bool m_ShowExportGameModal = false;
		char m_ExportGameName[128] = "";
		char m_ExportGameDir[512] = "";
		int  m_ExportGameW = 1280;
		int  m_ExportGameH = 720;
		bool m_ExportGameVsync = true;
		// Ship the game with the path tracer as its renderer (retro/pixelated
		// real-time RT). The rt settings are captured from the Raytracer
		// Settings panel at export time.
		bool m_ExportGameRaytraced = false;
		std::string m_ExportGameScene; // vfs path of the scene the game boots into
		PendingAction m_PendingAction = PendingAction::None;
		std::string m_PendingScenePath;

		// --- Unsaved-changes marker (window title "*") ---
		bool m_SceneDirty = false;
		std::string m_LastWindowTitle;

		// Set when a Lua script calls Engine.quit() during play; honored at the
		// top of on_update (stopping mid-script would destroy the script pool
		// while it's being iterated).
		bool m_QuitToEditorRequested = false;

		// --- Help windows ---
		bool m_ShowAboutModal = false;
		bool m_ShowControlsWindow = false;

		// --- Overlays ---
		bool m_ShowStats = false;        // View menu toggle: FPS / entities / samples
		float m_FlySpeedToastTimer = 0.0f; // shows fly speed briefly after scrolling

		// --- Panel visibility (per mode; Play shares Edit's set) ---
		// Edit defaults to the scene-building panels; Render swaps the content
		// browser/console out for the animation timeline, so the two modes feel
		// like different workspaces. Toggled in View > Panels, saved in the
		// engine config.
		struct PanelSet {
			bool hierarchy = true;
			bool inspector = true;
			bool browser = true;
			bool console = true;
			bool animation = true;
			bool raytracer_settings = true; // only drawn in Render mode anyway
		};
		PanelSet m_PanelsEdit{ .hierarchy = true, .inspector = true, .browser = true,
							   .console = true, .animation = false, .raytracer_settings = true };
		PanelSet m_PanelsRender{ .hierarchy = true, .inspector = true, .browser = false,
								 .console = false, .animation = true, .raytracer_settings = true };
		PanelSet& active_panels() { return m_SceneState == SceneState::Render ? m_PanelsRender : m_PanelsEdit; }

		// --- Animation (camera track + entity component tracks) ---
		SceneAnimation m_Animation;
		AnimationPanel m_AnimationPanel;
		bool m_ShowAnimRenderModal = false;
		bool m_IsRenderingAnim = false; // drives the frame loop in draw_modals
		int  m_AnimFps = 30;
		int  m_AnimRenderFrame = 0;
		int  m_AnimTotalFrames = 0;
		std::string m_AnimOutDir;

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

		// Play mode renders through the path tracer (toolbar "RT" toggle, set in
		// Edit mode). The raytracer starts/stops with play, renders one burst of
		// samples per frame from the scene camera, and the viewport shows its
		// output instead of the raster view.
		bool m_RaytracePlayMode = false;

		// --- UI Panels ---
		SceneHierarchyPanel m_HierarchyPanel;
		InspectorPanel m_InspectorPanel;
		ContentBrowserPanel m_BrowserPanel;
		ConsolePanel m_ConsolePanel;
		ViewportPanel m_ViewportPanel;
		ProjectHubPanel m_HubPanel;

		// --- Layout ---
		// Deferred layout IO: applied at the top of the next UI frame (vfs paths).
		std::string m_PendingLayoutSave;
		std::string m_PendingLayoutLoad;
		std::string m_PendingLayoutLoadMem; // ini text to load (session stash)
		bool m_AutoModeLayouts = true; // Edit/Render swap dock layouts automatically

		// In-session layout stash per mode: switching modes keeps the current
		// arrangement without touching disk (disk changes only via Save Layout).
		std::string m_LayoutMemEdit;
		std::string m_LayoutMemRender;

		// -- Command History --
		editor::CommandHistory m_CommandHistory;
	};

} // namespace editor