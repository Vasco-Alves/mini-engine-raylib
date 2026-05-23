#pragma once

#include <string>

namespace editor::utils {
	// Opens the native OS folder picker and returns the path. 
	// Returns an empty string if the user cancels.
	std::string select_folder_dialog(const std::string& title, const std::string& default_path);
}