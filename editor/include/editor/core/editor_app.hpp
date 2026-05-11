#pragma once

#include "editor/panels/scene_hierarchy_panel.hpp"
#include "editor/panels/content_browser_panel.hpp"

#include <mini-engine-raylib/core/application.hpp>
#include <mini-engine-raylib/ecs/components.hpp>

#include <raylib.h>
#include <filesystem>

namespace editor {

	enum class SceneState {
		Edit = 0,
		Play = 1
	};

	class EditorApp : public me::Application {
	public:
		void on_start() override;
		void on_update(float dt) override;
		void on_render() override;
		void on_shutdown() override;
		void on_resize(int width, int height) override;

	private:
		// --- UI & Styling ---
		void apply_theme();
		void draw_menu_bar();
		void draw_toolbar();
		void draw_modals();

		// --- Scene Management ---
		void new_scene();
		void save_scene() const;
		void on_play();
		void on_stop();

		// --- Project Hub ---
		void draw_project_hub();
		void create_project(const std::filesystem::path& path);
		void load_project(const std::filesystem::path& path);

		// --- Panels ---
		SceneHierarchyPanel m_HierarchyPanel;
		ContentBrowserPanel m_BrowserPanel;

		// --- State Variables ---
		bool m_ShowNewSceneModal = false;
		char m_NewSceneInput[256] = "my_new_scene";

		bool m_IsProjectLoaded = false;
		std::filesystem::path m_ProjectPath = "";
		char m_ProjectInputPath[512] = "C:/Dev/MyNewGame";

		std::string m_CurrentScenePath = "";
		SceneState m_SceneState = SceneState::Edit;

		// -- Viewport ---
		RenderTexture2D m_ViewportTexture;
		Vector2 m_ViewportBounds = { 1920.0f, 1080.0f };
		bool m_SceneViewFocused = false;
		bool m_IsFlying = false;

		// -- Camera --
		me::components::TransformComponent m_EditorCameraTransform = {
			{0.0f, 5.0f, 10.0f}, {-25.0f, 180.0f, 0.0f}, {1.0f, 1.0f, 1.0f}
		};
		me::components::CameraComponent m_EditorCamera;
	};

} // namespace editor