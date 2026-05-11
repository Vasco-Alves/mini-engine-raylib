#include "editor/panels/content_browser_panel.hpp"

#include <mini-engine-raylib/scene/scene_manager.hpp>
#include <mini-engine-raylib/core/engine.hpp>
#include <mini-engine-raylib/ecs/components.hpp>
#include <mini-engine-raylib/assets/assets.hpp>
#include <mini-ecs/registry.hpp>

#include <imgui.h>
#include <algorithm> // For std::replace

namespace editor {

	void ContentBrowserPanel::set_project_path(const std::filesystem::path& path) {
		m_ProjectPath = path;
		m_CurrentDirectory = m_ProjectPath / "assets";
	}

	void ContentBrowserPanel::on_imgui_render() {
		ImGui::Begin("Content Browser");

		// 1. Directory Path & Back Button
		if (m_CurrentDirectory != std::filesystem::path(m_ProjectPath / "assets")) {
			if (ImGui::Button("<- Back")) {
				m_CurrentDirectory = m_CurrentDirectory.parent_path();
			}
			ImGui::SameLine();
		}

		std::filesystem::path relativePath = std::filesystem::relative(m_CurrentDirectory, m_ProjectPath);
		ImGui::TextColored(ImVec4(0.44f, 0.37f, 0.61f, 1.0f), "%s", relativePath.string().c_str());
		ImGui::Separator();

		float padding = 16.0f;
		float thumbnailSize = 74.0f;
		float cellSize = thumbnailSize + padding;
		float panelWidth = ImGui::GetContentRegionAvail().x;
		int columnCount = (int)(panelWidth / cellSize);
		if (columnCount < 1) columnCount = 1;

		if (ImGui::BeginTable("ContentGrid", columnCount)) {

			for (auto& directoryEntry : std::filesystem::directory_iterator(m_CurrentDirectory)) {
				const auto& path = directoryEntry.path();
				std::string filenameString = path.filename().string();
				bool is_dir = directoryEntry.is_directory();
				std::string ext = path.extension().string();

				ImGui::TableNextColumn();
				ImGui::PushID(filenameString.c_str());

				// --- SMART ASSET RECOGNITION (Color Coding) ---
				ImVec4 iconColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
				const char* iconLabel = "[FILE]";

				if (is_dir) {
					iconLabel = "[DIR]";
					iconColor = ImVec4(0.9f, 0.7f, 0.2f, 1.0f); // Folder Yellow
				} else if (ext == ".json") {
					iconLabel = "[SCN]";
					iconColor = ImVec4(0.2f, 0.8f, 0.2f, 1.0f); // Scene Green
				} else if (ext == ".lua") {
					iconLabel = "[SCR]";
					iconColor = ImVec4(0.2f, 0.6f, 0.9f, 1.0f); // Script Blue
				} else if (ext == ".glb" || ext == ".obj") {
					iconLabel = "[MDL]";
					iconColor = ImVec4(0.9f, 0.4f, 0.2f, 1.0f); // Model Orange
				} else if (ext == ".png") {
					iconLabel = "[TEX]";
					iconColor = ImVec4(0.8f, 0.2f, 0.8f, 1.0f); // Texture Purple
				}

				// Draw the colored button
				ImGui::PushStyleColor(ImGuiCol_Text, iconColor);
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0)); // Transparent bg

				if (ImGui::Button(iconLabel, ImVec2(thumbnailSize, thumbnailSize))) {
					if (is_dir) m_CurrentDirectory /= path.filename(); // Dive into folder
				}

				ImGui::PopStyleColor(2);

				// --- INTERACTION (Double Clicks) ---
				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {

					// 1. Double Click a Scene to Load it
					if (ext == ".json") {
						me::scene_manager::load(path.string());

						// Trigger the Callbacks so EditorApp can update itself!
						if (on_scene_loaded) on_scene_loaded(path.string());
						if (on_entity_selection_cleared) on_entity_selection_cleared();
					}

					// 2. Double Click a Model to Spawn it into the level!
					else if (ext == ".glb" || ext == ".obj") {
						// Convert absolute Windows path to a safe VFS game:// path
						std::string relative_vfs = "game://" + std::filesystem::relative(path, m_ProjectPath / "assets").string();
						std::replace(relative_vfs.begin(), relative_vfs.end(), '\\', '/'); // Fix windows slashes

						auto& reg = me::get_registry();
						auto e = reg.create_entity(path.stem().string()); // Name entity after file
						e.add_component(me::components::TransformComponent{ {0,0,0}, {0,0,0}, {1,1,1} });
						e.add_component(me::components::Model3DComponent{
							me::assets::load_model(relative_vfs.c_str()),
							me::Color::white
							});
					}
				}

				// Draw the filename centered below the button
				ImGui::TextWrapped("%s", filenameString.c_str());

				ImGui::PopID();
			}
			ImGui::EndTable();
		}
		ImGui::End();
	}

}
