#pragma once

#include <string>

namespace me::logger {

	enum class LogLevel { Info, Warning, Error };

	// Just declare the functions, don't define them here!
	void info(const std::string& msg);
	void warn(const std::string& msg);
	void error(const std::string& msg);

} // namespace me::logger