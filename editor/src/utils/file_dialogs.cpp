#include "editor/utils/file_dialogs.hpp"
#include "editor/core/portable-file-dialogs.h"

namespace editor::utils {

	std::string select_folder_dialog(const std::string& title, const std::string& default_path) {
		return pfd::select_folder(title, default_path).result();
	}

    void open_folder_dialog(const std::string& path) {
        // Normalize separators first
        std::string normalized = path;

#if defined(_WIN32)
        // Windows explorer needs backslashes
        std::replace(normalized.begin(), normalized.end(), '/', '\\');
        std::string cmd = "explorer \"" + normalized + "\"";
        std::system(cmd.c_str());

#elif defined(__APPLE__)
        std::string cmd = "open \"" + normalized + "\"";
        std::system(cmd.c_str());

#elif defined(__linux__) || defined(__FreeBSD__)
        std::string cmd = "xdg-open \"" + normalized + "\"";
        std::system(cmd.c_str());
#endif
    }

}
