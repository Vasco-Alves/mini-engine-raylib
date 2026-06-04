#include "editor/panels/content_browser_panel.hpp"
#include "editor/utils/file_dialogs.hpp"
#include <mini-engine-raylib/scene/scene_manager.hpp>
#include <mini-engine-raylib/core/engine.hpp>
#include <mini-engine-raylib/ecs/components.hpp>
#include <mini-engine-raylib/assets/assets.hpp>
#include <mini-engine-raylib/core/file_system.hpp>
#include <mini-engine-raylib/core/events.hpp>
#include <mini-engine-raylib/audio/audio.hpp>
#include <mini-ecs/registry.hpp>
#include <imgui.h>
#include <algorithm>

namespace editor {

	void ContentBrowserPanel::set_project_path(const std::filesystem::path& path) {
		m_ProjectPath = path;
		m_CurrentDirectory = m_ProjectPath / "assets";
	}

	void ContentBrowserPanel::on_imgui_render() {
		ImGui::Begin("Content Browser");

		// Directory Path & Back Button
		if (m_CurrentDirectory != std::filesystem::path(m_ProjectPath / "assets")) {
			if (ImGui::Button("<- Back")) {
				m_CurrentDirectory = m_CurrentDirectory.parent_path();
			}
			ImGui::SameLine();
		}

		std::filesystem::path relativePath = std::filesystem::relative(m_CurrentDirectory, m_ProjectPath);
		ImGui::TextColored(ImVec4(0.44f, 0.37f, 0.61f, 1.0f), "%s", relativePath.string().c_str());
		ImGui::Separator();

		float scale = ImGui::GetIO().FontGlobalScale;
		float padding = 16.0f * scale;
		float thumbnailSize = 74.0f * scale;
		float cellSize = thumbnailSize + padding;
		float panelWidth = ImGui::GetContentRegionAvail().x;
		int columnCount = (int)(panelWidth / cellSize);
		if (columnCount < 1) columnCount = 1;

		// --- Right Click Empty Space ---
		if (ImGui::BeginPopupContextWindow("ContentBrowserBackground", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
			if (ImGui::MenuItem("New Folder")) {
				m_ShowNewFolderModal = true;
				strncpy(m_NewItemName, "NewFolder", sizeof(m_NewItemName));
			}

			if (ImGui::MenuItem("New Lua Script")) {
				m_ShowNewScriptModal = true;
				strncpy(m_NewItemName, "new_script", sizeof(m_NewItemName));
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Open in System Explorer")) {
				editor::utils::open_folder_dialog(
					std::filesystem::absolute(m_CurrentDirectory).string()
				);
			}
			ImGui::EndPopup();
		}

		if (ImGui::BeginTable("ContentGrid", columnCount)) {

			for (auto& directoryEntry : std::filesystem::directory_iterator(m_CurrentDirectory)) {
				const auto& path = directoryEntry.path();
				std::string filenameString = path.filename().string();
				bool is_dir = directoryEntry.is_directory();
				std::string ext = path.extension().string();

				ImGui::TableNextColumn();
				ImGui::PushID(filenameString.c_str());

				// --- File Recognition (Color Coding) ---
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
				} else if (ext == ".wav" || ext == ".ogg" || ext == ".mp3") {
					iconLabel = "[SND]";
					iconColor = ImVec4(0.2f, 0.9f, 0.8f, 1.0f); // Audio Teal
				}

				ImGui::PushStyleColor(ImGuiCol_Text, iconColor);
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0)); // Transparent bg

				if (ImGui::Button(iconLabel, ImVec2(thumbnailSize, thumbnailSize))) {
					if (is_dir) m_CurrentDirectory /= path.filename(); // Dive into folder
				}
				ImGui::PopStyleColor(2);

				// --- RIGHT CLICK SPECIFIC ITEM (Delete Menu) ---
				if (ImGui::BeginPopupContextItem()) {
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.2f, 0.2f, 1.0f)); // Red text
					if (ImGui::MenuItem("Delete Item")) {
						me::fs::remove_all(path.string());
					}
					ImGui::PopStyleColor();
					ImGui::EndPopup();
				}

				// --- DRAG AND DROP SOURCE ---
				if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
					std::string relative_vfs = "game://" + std::filesystem::relative(path, m_ProjectPath / "assets").string();
					std::replace(relative_vfs.begin(), relative_vfs.end(), '\\', '/');

					const char* itemPath = relative_vfs.c_str();
					ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", itemPath, (strlen(itemPath) + 1) * sizeof(char));
					ImGui::Text("Dragging %s", filenameString.c_str());
					ImGui::EndDragDropSource();
				}

				// --- DOUBLE CLICKS ---
				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
					if (ext == ".json") {

						// 1. Load the scene via the Scene Manager
						me::scene_manager::load(path.string());

						// 2. Send Events
						me::get_event_bus().publish<me::events::EntitySelectedEvent>(0xFFFFFFFF);

					} else if (ext == ".glb" || ext == ".obj") {
						std::string relative_vfs = "game://" + std::filesystem::relative(path, m_ProjectPath / "assets").string();
						std::replace(relative_vfs.begin(), relative_vfs.end(), '\\', '/');

						auto& reg = me::get_registry();
						auto e = reg.create_entity();
						e.add_component(me::components::TagComponent{ path.stem().string() });
						e.add_component(me::components::TransformComponent{ {0,0,0}, {0,0,0}, {1,1,1} });
						e.add_component(me::components::Model3DComponent{
							me::assets::load_model(relative_vfs.c_str()),
							me::Color::white
							});
					} else if (ext == ".wav" || ext == ".ogg" || ext == ".mp3") {
						// Instantly preview audio files at 100% volume in the center of the stereo field
						std::string relative_vfs = "game://" + std::filesystem::relative(path, m_ProjectPath / "assets").string();
						std::replace(relative_vfs.begin(), relative_vfs.end(), '\\', '/');

						me::audio::SoundId preview_snd = me::audio::load(relative_vfs.c_str());
						me::audio::play(preview_snd, 1.0f, 1.0f, 0.5f);
					}
				}

				ImGui::TextWrapped("%s", filenameString.c_str());
				ImGui::PopID();
			}
			ImGui::EndTable();
		}

		// ==========================================
		// MODALS
		// ==========================================

		// --- NEW FOLDER MODAL ---
		if (m_ShowNewFolderModal) ImGui::OpenPopup("Create New Folder");
		ImGui::SetNextWindowPos(ImVec2(GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal("Create New Folder", &m_ShowNewFolderModal, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("Enter folder name:");
			ImGui::InputText("##FolderName", m_NewItemName, sizeof(m_NewItemName));
			ImGui::Dummy(ImVec2(0, 10));

			if (ImGui::Button("Create", ImVec2(120, 0))) {
				me::fs::create_directory((m_CurrentDirectory / m_NewItemName).string());
				m_ShowNewFolderModal = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120, 0))) {
				m_ShowNewFolderModal = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		// --- NEW SCRIPT MODAL ---
		if (m_ShowNewScriptModal) ImGui::OpenPopup("Create New Script");
		ImGui::SetNextWindowPos(ImVec2(GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal("Create New Script", &m_ShowNewScriptModal, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("Enter script name:");
			ImGui::InputText("##ScriptName", m_NewItemName, sizeof(m_NewItemName));
			ImGui::Dummy(ImVec2(0, 10));

			if (ImGui::Button("Create", ImVec2(120, 0))) {
				std::string filename = m_NewItemName;
				if (filename.find(".lua") == std::string::npos) filename += ".lua";

				// Write the boilerplate Lua
				std::string boilerplate =
					"-- " + filename + "\n\n"
					"function start(entity)\n\nend\n\n"
					"function update(entity, dt)\n\nend\n";

				me::fs::write_text((m_CurrentDirectory / filename).string(), boilerplate);

				m_ShowNewScriptModal = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120, 0))) {
				m_ShowNewScriptModal = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		ImGui::End();
	}

}
