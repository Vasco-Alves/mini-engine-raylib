#pragma once

#include <filesystem>
#include <string>
#include <functional>

namespace editor {

	class ContentBrowserPanel {
	public:
		ContentBrowserPanel() = default;

		void set_project_path(const std::filesystem::path& path);

		std::filesystem::path get_current_directory() const { return m_CurrentDirectory; }

		void on_imgui_render();

	private:
		std::filesystem::path m_ProjectPath = "";
		std::filesystem::path m_CurrentDirectory = "";

		// --- Modal State Tracking ---
		bool m_ShowNewFolderModal = false;
		bool m_ShowNewScriptModal = false;
		char m_NewItemName[256] = "";
	};

} // namespace editor
