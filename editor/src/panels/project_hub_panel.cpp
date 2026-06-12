#include "editor/panels/project_hub_panel.hpp"
#include "editor/utils/file_dialogs.hpp"

#include <imgui.h>
#include <rlImGui.h>
#include <mini-engine-raylib/core/file_system.hpp>
#include <algorithm>
#include <cstdio>
#include <system_error>

namespace editor {

	void ProjectHubPanel::unload_background() {
		if (m_Background.id != 0) {
			UnloadTexture(m_Background);
			m_Background = {};
		}
		m_TriedLoadBackground = false; // reload if the hub ever shows again
	}

	void ProjectHubPanel::on_imgui_render() {
		// ------------------------------------------------------------------
		// Background: a branded gradient image stretched to the window
		// (falls back to a plain dark clear when the asset is missing).
		// ------------------------------------------------------------------
		if (!m_TriedLoadBackground) {
			m_TriedLoadBackground = true;
			m_Background = LoadTexture("assets/hub_bg.png");
		}

		ClearBackground({ 18, 18, 23, 255 });
		if (m_Background.id != 0) {
			Rectangle src{ 0.0f, 0.0f, (float)m_Background.width, (float)m_Background.height };
			Rectangle dst{ 0.0f, 0.0f, (float)GetScreenWidth(), (float)GetScreenHeight() };
			DrawTexturePro(m_Background, src, dst, { 0.0f, 0.0f }, 0.0f, WHITE);
		}

		rlImGuiBegin();

		float scale = ImGui::GetIO().FontGlobalScale;
		ImGui::SetNextWindowPos(ImVec2(GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(780.0f * scale, 400.0f * scale));

		ImGui::Begin("Project Hub", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

		ImGui::TextColored(ImVec4(0.74f, 0.58f, 0.98f, 1.0f), "Mini Engine | Project Hub");
		ImGui::Separator();
		ImGui::Dummy(ImVec2(0, 5));

		ImGui::Columns(2, "HubColumns", false);
		ImGui::SetColumnWidth(0, 400.0f * scale);

		// ------------------------------------------------------------------
		// NEW PROJECT: name + location -> creates <location>/<name>/ and
		// scaffolds the project (assets folders + demo scene) inside it.
		// ------------------------------------------------------------------
		ImGui::TextDisabled("New Project");

		ImGui::Text("Project Name:");
		ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
		ImGui::InputText("##Name", m_ProjectName, sizeof(m_ProjectName));
		ImGui::PopItemWidth();

		ImGui::Text("Location:");
		ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 90.0f);
		ImGui::InputText("##Location", m_ProjectLocation, sizeof(m_ProjectLocation));
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (ImGui::Button("Browse", ImVec2(80, 0))) {
			std::string dir = editor::utils::select_folder_dialog("Choose where to create the project", m_ProjectLocation);
			if (!dir.empty()) {
				std::replace(dir.begin(), dir.end(), '\\', '/');
				snprintf(m_ProjectLocation, sizeof(m_ProjectLocation), "%s", dir.c_str());
			}
		}

		// Live preview of the folder the project will live in.
		std::string name = m_ProjectName;
		std::string location = m_ProjectLocation;
		std::string full_path = (location.empty() || name.empty())
			? "" : location + "/" + name;
		if (!full_path.empty()) {
			ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
			ImGui::TextColored(ImVec4(0.45f, 0.45f, 0.5f, 1.0f), "Will create: %s", full_path.c_str());
			ImGui::PopTextWrapPos();
		}

		ImGui::Dummy(ImVec2(0, 8));

		if (ImGui::Button("Create Project", ImVec2(ImGui::GetContentRegionAvail().x, 36))) {
			m_Error.clear();
			if (name.empty()) {
				m_Error = "Enter a project name.";
			} else if (name.find('/') != std::string::npos || name.find('\\') != std::string::npos) {
				m_Error = "The project name can't contain slashes.";
			} else if (location.empty()) {
				m_Error = "Choose a location for the project.";
			} else {
				std::error_code ec;
				std::filesystem::create_directories(full_path, ec);
				if (ec || !std::filesystem::exists(full_path)) {
					m_Error = "Could not create the project folder - check the path and permissions.";
				} else if (me::fs::exists((std::filesystem::path(full_path) / "assets").string())) {
					// A project already lives there — just open it.
					if (on_project_open) on_project_open(full_path);
				} else {
					if (on_project_create) on_project_create(full_path);
				}
			}
		}

		// ------------------------------------------------------------------
		// OPEN EXISTING: browse straight to a project folder.
		// ------------------------------------------------------------------
		ImGui::Dummy(ImVec2(0, 6));
		if (ImGui::Button("Open Existing Project...", ImVec2(ImGui::GetContentRegionAvail().x, 30))) {
			m_Error.clear();
			std::string dir = editor::utils::select_folder_dialog("Open a project folder", location);
			if (!dir.empty()) {
				std::replace(dir.begin(), dir.end(), '\\', '/');
				if (me::fs::exists((std::filesystem::path(dir) / "assets").string())) {
					if (on_project_open) on_project_open(dir);
				} else {
					m_Error = "That folder doesn't look like a project (no assets/ inside).";
				}
			}
		}

		// Render the error message OUTSIDE the button click events
		if (!m_Error.empty()) {
			ImGui::Dummy(ImVec2(0, 5));
			ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
			ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s", m_Error.c_str());
			ImGui::PopTextWrapPos();
		}

		ImGui::NextColumn();
		ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Recent Projects");
		ImGui::BeginChild("RecentsList", ImVec2(0, 0), true);

		if (m_RecentProjects.empty()) {
			ImGui::TextDisabled("No recent projects found.");
		} else {
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
			float col_w = ImGui::GetContentRegionAvail().x;

			std::string remove_path; // deferred: the callback mutates this list

			for (const auto& proj_path : m_RecentProjects) {
				std::string proj_name = std::filesystem::path(proj_path).filename().string();
				bool missing = !std::filesystem::exists(proj_path);
				ImGui::PushID(proj_path.c_str());

				ImVec2 start_pos = ImGui::GetCursorPos();
				if (ImGui::Selectable("##RecentProj", false, 0, ImVec2(col_w, 40.0f))) {
					if (missing) {
						// Don't crash into a folder that's gone — report and forget it.
						m_Error = "Project not found: " + proj_path + " (removed from the list)";
						remove_path = proj_path;
					} else {
						if (on_project_open) on_project_open(proj_path);
					}
				}
				ImVec2 end_pos = ImGui::GetCursorPos();

				ImGui::SetCursorPos(ImVec2(start_pos.x + 5.0f, start_pos.y + 4.0f));
				if (missing) {
					ImGui::TextColored(ImVec4(0.55f, 0.45f, 0.45f, 1.0f), "%s  (not found)", proj_name.c_str());
				} else {
					ImGui::Text("%s", proj_name.c_str());
				}
				ImGui::SetCursorPos(ImVec2(start_pos.x + 5.0f, start_pos.y + 20.0f));
				ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "%s", proj_path.c_str());

				ImGui::SetCursorPos(end_pos);
				ImGui::Dummy(ImVec2(0, 4.0f));
				ImGui::PopID();
			}
			ImGui::PopStyleVar();

			if (!remove_path.empty() && on_recent_remove) on_recent_remove(remove_path);
		}

		ImGui::EndChild();
		ImGui::Columns(1);
		ImGui::End();
		rlImGuiEnd();
	}
}
