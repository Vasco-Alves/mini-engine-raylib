#pragma once

#include "mini-engine-raylib/core/vfs.hpp"

#include <string>
#include <filesystem>
#include <fstream>

namespace me::fs {

	// Helper to automatically resolve VFS paths before doing OS operations
	inline std::filesystem::path resolve_if_virtual(const std::string& path) {
		if (path.find("://") != std::string::npos) {
			return me::vfs::resolve(path);
		}
		return path;
	}

	// --- OS File Operations ---

	inline bool exists(const std::string& path) {
		return std::filesystem::exists(resolve_if_virtual(path));
	}

	inline void remove(const std::string& path) {
		std::filesystem::path physical = resolve_if_virtual(path);
		if (std::filesystem::exists(physical)) {
			std::filesystem::remove(physical);
		}
	}

	inline void create_directory(const std::string& path) {
		std::filesystem::create_directories(resolve_if_virtual(path));
	}

	inline void remove_all(const std::string& path) {
		std::filesystem::path physical = resolve_if_virtual(path);
		if (std::filesystem::exists(physical)) {
			std::filesystem::remove_all(physical);
		}
	}

	inline std::string read_text(const std::string& path) {
		std::filesystem::path physical = resolve_if_virtual(path);
		std::ifstream in(physical);

		if (in.is_open()) {
			std::stringstream buffer;
			buffer << in.rdbuf(); // Dump the entire file into the string stream
			return buffer.str();
		}

		return "";
	}

	inline void write_text(const std::string& path, const std::string& content) {
		std::filesystem::path physical = resolve_if_virtual(path);
		std::ofstream out(physical);
		if (out.is_open()) {
			out << content;
			out.close();
		}
	}

} // namespace me::fs
