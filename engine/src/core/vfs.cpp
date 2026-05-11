#include "mini-engine-raylib/core/vfs.hpp"

#include <unordered_map>
#include <filesystem>

namespace me::vfs {

	namespace {
		// TODO: once vfspp gets integrated, replace this map with vfspp::VirtualFileSystem
		std::unordered_map<std::string, std::string> s_mounts;
	}

	void mount(const std::string& scheme, const std::string& physical_path) {
		std::filesystem::path p = physical_path;
		// Store it as "scheme://" so it's easy to search for
		s_mounts[scheme + "://"] = p.lexically_normal().string();
	}

	std::string resolve(const std::string& virtual_path) {
		// Find which scheme this path uses
		for (const auto& [scheme, physical] : s_mounts) {
			if (virtual_path.find(scheme) == 0) {
				// Strip the "engine://" part
				std::string relative = virtual_path.substr(scheme.length());

				// Combine the physical mount point with the relative path
				std::filesystem::path full = std::filesystem::path(physical) / relative;
				return full.lexically_normal().string();
			}
		}

		// Fallback: If no scheme is provided, assume it's just a raw physical path
		return virtual_path;
	}

} // namespace me::vfs