#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <functional>

namespace editor {

	class ProjectHubPanel {
	public:
		void on_imgui_render();

		void set_recent_projects(const std::vector<std::string>& recents) { m_RecentProjects = recents; }

		// Callbacks back to the Editor App
		std::function<void(const std::filesystem::path&)> on_project_open;
		std::function<void(const std::filesystem::path&)> on_project_create;

	private:
		char m_ProjectInputPath[256] = "";
		std::vector<std::string> m_RecentProjects;
	};

}