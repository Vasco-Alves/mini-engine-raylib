#include "editor/panels/project_hub_panel.hpp"
#include "editor/utils/file_dialogs.hpp"

#include <imgui.h>
#include <rlImGui.h>
#include <mini-engine-raylib/core/file_system.hpp>
#include <algorithm>

namespace editor {

	void ProjectHubPanel::on_imgui_render() {
		ClearBackground(BLACK);
		rlImGuiBegin();

		float scale = ImGui::GetIO().FontGlobalScale;
		ImGui::SetNextWindowPos(ImVec2(GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(750.0f * scale, 350.0f * scale));

		ImGui::Begin("Project Hub", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

		ImGui::TextColored(ImVec4(0.74f, 0.58f, 0.98f, 1.0f), "Mini Engine | Project Hub");
		ImGui::Separator();
		ImGui::Dummy(ImVec2(0, 5));

		ImGui::Columns(2, "HubColumns", false);
		ImGui::SetColumnWidth(0, 400.0f * scale);

		ImGui::Text("Project Path:");
		ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 90.0f);
		ImGui::InputText("##Path", m_ProjectInputPath, sizeof(m_ProjectInputPath));
		ImGui::PopItemWidth();
		ImGui::SameLine();

		if (ImGui::Button("Browse", ImVec2(80, 0))) {
			std::string dir = editor::utils::select_folder_dialog("Choose Project Folder", m_ProjectInputPath);
			if (!dir.empty()) {
				std::replace(dir.begin(), dir.end(), '\\', '/');
				strncpy(m_ProjectInputPath, dir.c_str(), sizeof(m_ProjectInputPath) - 1);
			}
		}

		ImGui::Dummy(ImVec2(0, 20));

		if (ImGui::Button("Open Project", ImVec2(ImGui::GetContentRegionAvail().x, 40))) {
			std::string target_path = m_ProjectInputPath;
			if (!me::fs::exists(target_path)) me::fs::create_directory(target_path);

			if (me::fs::exists((std::filesystem::path(target_path) / "assets").string())) {
				if (on_project_open) on_project_open(target_path);
			} else {
				if (on_project_create) on_project_create(target_path);
			}
		}

		ImGui::NextColumn();
		ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Recent Projects");
		ImGui::BeginChild("RecentsList", ImVec2(0, 0), true);

		if (m_RecentProjects.empty()) {
			ImGui::TextDisabled("No recent projects found.");
		} else {
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
			float col_w = ImGui::GetContentRegionAvail().x;

			for (const auto& proj_path : m_RecentProjects) {
				std::string proj_name = std::filesystem::path(proj_path).filename().string();
				ImGui::PushID(proj_path.c_str());

				ImVec2 start_pos = ImGui::GetCursorPos();
				if (ImGui::Selectable("##RecentProj", false, 0, ImVec2(col_w, 40.0f))) {
					if (on_project_open) on_project_open(proj_path);
				}
				ImVec2 end_pos = ImGui::GetCursorPos();

				ImGui::SetCursorPos(ImVec2(start_pos.x + 5.0f, start_pos.y + 4.0f));
				ImGui::Text("%s", proj_name.c_str());
				ImGui::SetCursorPos(ImVec2(start_pos.x + 5.0f, start_pos.y + 20.0f));
				ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "%s", proj_path.c_str());

				ImGui::SetCursorPos(end_pos);
				ImGui::Dummy(ImVec2(0, 4.0f));
				ImGui::PopID();
			}
			ImGui::PopStyleVar();
		}

		ImGui::EndChild();
		ImGui::Columns(1);
		ImGui::End();
		rlImGuiEnd();
	}
}
