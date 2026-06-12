#pragma once

#include <raylib.h>

#include <string>
#include <vector>
#include <filesystem>
#include <functional>

namespace editor {

	class ProjectHubPanel {
	public:
		void on_imgui_render();

		// Frees the background texture once a project is loaded (the hub is gone).
		void unload_background();

		void set_recent_projects(const std::vector<std::string>& recents) { m_RecentProjects = recents; }

		// Callbacks back to the Editor App
		std::function<void(const std::filesystem::path&)> on_project_open;
		std::function<void(const std::filesystem::path&)> on_project_create;
		// A recent entry pointed at a folder that no longer exists — drop it.
		std::function<void(const std::string&)> on_recent_remove;

	private:
		char m_ProjectName[128] = "MyGame";
		char m_ProjectLocation[512] = "";
		std::vector<std::string> m_RecentProjects;
		std::string m_Error; // shown under the buttons; empty = no error

		Texture2D m_Background{};
		bool m_TriedLoadBackground = false;
	};

}
