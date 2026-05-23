#include "mini-engine-raylib/core/logger.hpp"
#include "mini-engine-raylib/core/events.hpp"

namespace me::logger {

	void info(const std::string& msg) {
		me::get_event_bus().publish<me::events::LogEvent>(msg, LogLevel::Info);
	}

	void warn(const std::string& msg) {
		me::get_event_bus().publish<me::events::LogEvent>(msg, LogLevel::Warning);
	}

	void error(const std::string& msg) {
		me::get_event_bus().publish<me::events::LogEvent>(msg, LogLevel::Error);
	}

} // namespace me::logger