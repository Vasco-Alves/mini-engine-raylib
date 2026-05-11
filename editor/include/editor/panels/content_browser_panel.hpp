#pragma once

#include <filesystem>
#include <string>
#include <functional>

namespace editor {

	class ContentBrowserPanel {
	public:
		ContentBrowserPanel() = default;

		void set_project_path(const std::filesystem::path& path);

		void on_imgui_render();

		// --- Callbacks ---
		// The main Editor App will listen to these to update its own state
		std::function<void(const std::string&)> on_scene_loaded;
		std::function<void()> on_entity_selection_cleared;

	private:
		std::filesystem::path m_ProjectPath = "";
		std::filesystem::path m_CurrentDirectory = "";
	};

} // namespace editor