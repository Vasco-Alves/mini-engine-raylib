#include "editor/panels/console_panel.hpp"

namespace editor {

	std::vector<LogMessage> ConsolePanel::s_Messages;

	void ConsolePanel::add_log(const std::string& message, me::logger::LogLevel level) {
		s_Messages.push_back({ message, level });

		if (s_Messages.size() > 1000) {
			s_Messages.erase(s_Messages.begin());
		}
	}

	void ConsolePanel::clear() {
		s_Messages.clear();
	}

	void ConsolePanel::on_imgui_render() {
		ImGui::Begin("Console");

		// Toolbar
		if (ImGui::Button("Clear")) clear();
		ImGui::SameLine();
		ImGui::Checkbox("Auto-Scroll", &m_AutoScroll);
		ImGui::Separator();

		// Scrollable Log Region
		const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
		ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar);

		for (const auto& msg : s_Messages) {

			ImVec4 color;

			if (msg.level == me::logger::LogLevel::Warning) {
				color = ImVec4(0.9f, 0.7f, 0.2f, 1.0f); // Warning: Yellow
			} else if (msg.level == me::logger::LogLevel::Error) {
				color = ImVec4(0.9f, 0.2f, 0.2f, 1.0f); // Error: Red
			} else {
				if (msg.text.find("[LUA]") != std::string::npos) {
					color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // Lua: White
				} else {
					color = ImVec4(0.2f, 0.8f, 0.2f, 1.0f); // Engine C++: Green
				}
			}

			ImGui::PushStyleColor(ImGuiCol_Text, color);
			ImGui::TextUnformatted(msg.text.c_str());
			ImGui::PopStyleColor();
		}

		// Keep scrolled at the bottom if new messages arrive
		if (m_AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
			ImGui::SetScrollHereY(1.0f);
		}

		ImGui::EndChild();
		ImGui::End();
	}
}
