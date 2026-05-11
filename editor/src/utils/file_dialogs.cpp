#include "editor/utils/file_dialogs.hpp"
#include "editor/core/portable-file-dialogs.h"

namespace editor::utils {

	std::string select_folder_dialog(const std::string& title, const std::string& default_path) {
		return pfd::select_folder(title, default_path).result();
	}

}
