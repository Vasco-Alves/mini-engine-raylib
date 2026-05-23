#pragma once

#include <string>
#include <vector>
#include <imgui.h>

#include <mini-engine-raylib/core/logger.hpp> 

namespace editor {

	struct LogMessage {
		std::string text;
		me::logger::LogLevel level;
	};

	class ConsolePanel {
	public:
		ConsolePanel() = default;

		void on_imgui_render();

		static void add_log(const std::string& message, me::logger::LogLevel level = me::logger::LogLevel::Info);
		static void clear();

	private:
		static std::vector<LogMessage> s_Messages;
		bool m_AutoScroll = true;
	};

} // namespace editor
